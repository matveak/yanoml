#pragma once

#include <QDialog>
#include <QVector>
#include <QUrl>
#include <QList>
#include <QHash>
#include <QIcon>
#include <QStringList>

#include "modrinthapi.h"

class QLineEdit;
class QPushButton;
class QScrollArea;
class QWidget;
class QLabel;
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
                         QLabel** summaryOut, QWidget** contentOut, bool expanded);
    void updateSidebarSummaries();
    void refreshSummaryVisibility();

    void buildCategoryList();
    void rebuildVersionList();
    void rebuildLoaderList();

    void selectVersion(const QString& version);
    void selectLoader(const QString& loader);
    void selectCategory(const QString& category);
    void selectEnvironment(const QString& environment);

    void onCategoriesReceived(const QVector<TagInfo>& categories);
    void onLoadersReceived(const QVector<TagInfo>& loaders);
    void updateCategoryIcons();
    void updateLoaderIcons();

    // SVG-иконка из маркапа Modrinth, перекрашенная в нужный цвет
    static QIcon makeSvgIcon(const QString& svg, const QString& colorHex, int px = 18);

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

    // ── Сайдбар: версии ────────────────────────────────────────────────
    QLineEdit* versionSearch = nullptr;
    QVBoxLayout* versionListLayout = nullptr;
    QList<QPushButton*> versionButtons;
    QStringList allVersions;
    bool showAllVersions = false;
    QString selectedVersion;       // "" = любая

    // ── Сайдбар: загрузчики ────────────────────────────────────────────
    QVBoxLayout* loaderListLayout = nullptr;
    QList<QPushButton*> loaderButtons;
    QVector<TagInfo> loaderTags;
    QHash<QString, QString> loaderIcons;   // слаг -> svg
    bool showAllLoaders = false;
    QString selectedLoader;        // "" = любой (нижний регистр)

    // ── Сайдбар: категории/окружение ───────────────────────────────────
    QVBoxLayout* categoryListLayout = nullptr;
    QList<QPushButton*> categoryButtons;
    QHash<QString, QString> categoryIcons;  // слаг -> svg
    QPushButton* clientButton = nullptr;
    QPushButton* serverButton = nullptr;

    // Чипы-сводки под заголовками свёрнутых секций
    QLabel* versionSummary = nullptr;
    QLabel* loaderSummary = nullptr;
    QLabel* categorySummary = nullptr;
    QLabel* environmentSummary = nullptr;

    // Контент-виджеты секций (для определения свёрнуто/развёрнуто)
    QWidget* versionContent = nullptr;
    QWidget* loaderContent = nullptr;
    QWidget* categoryContent = nullptr;
    QWidget* environmentContent = nullptr;

    QString selectedCategory;      // "" = любая
    QString selectedEnvironment;   // "" = любая ("client"/"server")

    // Карточки -> полный Mod (для открытия страницы мода в лаунчере)
    QHash<QString, Mod> modById;

    ModrithAPI* api = nullptr;
    SettingsWindow* settingsWindow = nullptr;
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};
