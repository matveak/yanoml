#include "modwindow.h"
#include "moddetailwindow.h"
#include "settingswindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QFrame>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QComboBox>
#include <QCheckBox>
#include <QNetworkReply>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QDateTime>
#include <QSet>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QByteArray>
#include <QRegularExpression>
#include <functional>

// ── Палитра в стиле Modrinth (тёмная тема) ───────────────────────────────────
namespace {
const char* kBg        = "#16181C";
const char* kPanel     = "#26292F";
const char* kPanelHi   = "#2F333A";
const char* kBorder    = "#3A3E45";
const char* kText      = "#E8EAED";
const char* kTextDim   = "#9CA3AF";
const char* kAccent    = "#1BD96A"; // фирменный зелёный Modrinth

QString prettyCategory(const QString& slug)
{
    QString s = slug;
    s.replace('-', ' ');
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

QString scrollbarStyle()
{
    return QString(
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 5px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #4A4F57; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(kBorder);
}

// Кликабельная «шапка» секции: обычный QFrame с layout (рендерится надёжно,
// в отличие от layout внутри QPushButton) + клик через std::function.
class ClickableFrame : public QFrame
{
public:
    using QFrame::QFrame;
    std::function<void()> onClick;
protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && onClick && rect().contains(e->pos()))
            onClick();
        QFrame::mouseReleaseEvent(e);
    }
};

bool isLoaderTag(const QString& tag)
{
    static const QSet<QString> loaders = {
        "fabric", "forge", "neoforge", "quilt", "liteloader", "modloader",
        "rift", "bukkit", "spigot", "paper", "purpur", "sponge", "folia",
        "velocity", "bungeecord", "waterfall", "datapack", "minecraft"
    };
    return loaders.contains(tag.toLower());
}
} // namespace

ModWindow::ModWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Менеджер модов — Modrinth");
    resize(1400, 900);
    setStyleSheet(QString("QDialog { background-color: %1; color: %2; }").arg(kBg, kText));

    api = new ModrithAPI(this);

    // Корневой макет: слева контент, справа сайдбар фильтров
    QHBoxLayout* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(16);

    // ── Левая колонка ────────────────────────────────────────────────────────
    QWidget* leftColumn = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftColumn);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(12);

    // Строка поиска
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Поиск модов...");
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setFixedHeight(42);
    searchEdit->setStyleSheet(QString(
        "QLineEdit { background-color: %1; border: 1px solid %2; border-radius: 8px;"
        " padding: 0 12px; color: %3; }"
        "QLineEdit:focus { border: 1px solid %4; }")
        .arg(kPanel, kBorder, kText, kAccent));

    searchButton = new QPushButton("Поиск", this);
    searchButton->setFixedHeight(42);
    searchButton->setFixedWidth(120);
    searchButton->setCursor(Qt::PointingHandCursor);
    searchButton->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: #0A0A0A; border-radius: 8px;"
        " font-weight: bold; }"
        "QPushButton:hover { background-color: #19c460; }").arg(kAccent));

    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(searchButton);
    leftLayout->addLayout(searchLayout);

    // Строка сортировки и количества
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    QString comboStyle = QString(
        "QComboBox { background-color: %1; border: 1px solid %2; border-radius: 8px;"
        " padding: 6px 10px; color: %3; }"
        "QComboBox:hover { border: 1px solid %4; }"
        "QComboBox QAbstractItemView { background-color: %1; color: %3;"
        " selection-background-color: %4; selection-color: #0A0A0A; }")
        .arg(kPanel, kBorder, kText, kAccent);

    QLabel* sortLbl = new QLabel("Сортировка:");
    sortLbl->setStyleSheet(QString("color: %1;").arg(kTextDim));
    sortCombo = new QComboBox(this);
    // Порядок строго соответствует enum SortOrder
    sortCombo->addItems({"Релевантность", "Загрузки", "Подписки", "Новизна", "Обновлённые"});
    sortCombo->setCurrentIndex(1); // Загрузки
    sortCombo->setStyleSheet(comboStyle);
    sortCombo->setFixedWidth(170);

    QLabel* viewLbl = new QLabel("Показывать:");
    viewLbl->setStyleSheet(QString("color: %1;").arg(kTextDim));
    viewCombo = new QComboBox(this);
    viewCombo->addItems({"10", "20", "50", "100"});
    viewCombo->setCurrentIndex(1); // 20
    viewCombo->setStyleSheet(comboStyle);
    viewCombo->setFixedWidth(90);

    controlsLayout->addWidget(sortLbl);
    controlsLayout->addWidget(sortCombo);
    controlsLayout->addSpacing(12);
    controlsLayout->addWidget(viewLbl);
    controlsLayout->addWidget(viewCombo);
    controlsLayout->addStretch(1);
    leftLayout->addLayout(controlsLayout);

    // Полоса активных фильтров (чипы)
    chipsBar = new QWidget(this);
    chipsLayout = new QHBoxLayout(chipsBar);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(8);
    chipsBar->setVisible(false);
    leftLayout->addWidget(chipsBar);

    // Карточки модов
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(scrollbarStyle());

    cardsWidget = new QWidget();
    cardsLayout = new QVBoxLayout(cardsWidget);
    cardsLayout->setSpacing(12);
    cardsLayout->setContentsMargins(0, 0, 8, 0);
    cardsLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidget(cardsWidget);
    leftLayout->addWidget(scrollArea, 1);

    rootLayout->addWidget(leftColumn, 1);

    // ── Правый сайдбар ─────────────────────────────────────────────────────────
    rootLayout->addWidget(buildSidebar());

    // ── Сигналы ───────────────────────────────────────────────────────────────
    connect(searchButton, &QPushButton::clicked, this, &ModWindow::applyFilters);
    connect(searchEdit, &QLineEdit::returnPressed, this, &ModWindow::applyFilters);
    connect(sortCombo, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);
    connect(viewCombo, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);

    connect(api, &ModrithAPI::AvailableVersions, this, &ModWindow::onAvailableVersions);
    connect(api, &ModrithAPI::CategoriesReceived, this, &ModWindow::onCategoriesReceived);
    connect(api, &ModrithAPI::LoadersReceived, this, &ModWindow::onLoadersReceived);
    connect(api, &ModrithAPI::ModList, this, &ModWindow::addModCards);
    connect(api, &ModrithAPI::OnError, this, [this](const QString& error){
        QMessageBox::warning(this, "Modrinth", error);
    });

    // Подтягиваем SVG-иконки категорий и загрузчиков из Modrinth.
    api->fetchCategories();
    api->fetchLoaders();

    updateVersionsForLoader();
}

// ===================== SIDEBAR =====================
QWidget* ModWindow::buildSidebar()
{
    QScrollArea* sideScroll = new QScrollArea(this);
    sideScroll->setWidgetResizable(true);
    sideScroll->setFrameShape(QFrame::NoFrame);
    sideScroll->setFixedWidth(300);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setStyleSheet(scrollbarStyle());

    QWidget* panel = new QWidget();
    panel->setStyleSheet("background: transparent;");

    QVBoxLayout* side = new QVBoxLayout(panel);
    side->setContentsMargins(0, 0, 8, 0);
    side->setSpacing(10);

    const QString rowStyle = QString(
        "QPushButton { text-align: left; padding: 7px 10px; border: none;"
        " border-radius: 8px; color: %1; background: transparent; }"
        "QPushButton:hover { background-color: %2; color: %3; }"
        "QPushButton:checked { background-color: %4; color: #0A0A0A; font-weight: bold; }")
        .arg(kTextDim, kPanelHi, kText, kAccent);

    // ── Версия игры: поиск + список + «Показать все версии» ─────────────────────
    QWidget* verContent = new QWidget();
    QVBoxLayout* verL = new QVBoxLayout(verContent);
    verL->setContentsMargins(14, 0, 14, 14);
    verL->setSpacing(8);

    versionSearch = new QLineEdit(verContent);
    versionSearch->setPlaceholderText("Найти версию...");
    versionSearch->setClearButtonEnabled(true);
    versionSearch->setStyleSheet(QString(
        "QLineEdit { background-color: %1; border: 1px solid %2; border-radius: 8px;"
        " padding: 6px 10px; color: %3; }"
        "QLineEdit:focus { border: 1px solid %4; }").arg(kBg, kBorder, kText, kAccent));
    verL->addWidget(versionSearch);

    QScrollArea* verScroll = new QScrollArea(verContent);
    verScroll->setWidgetResizable(true);
    verScroll->setFrameShape(QFrame::NoFrame);
    verScroll->setMaximumHeight(240);
    verScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    verScroll->setStyleSheet(scrollbarStyle());
    QWidget* verListWrap = new QWidget();
    verListWrap->setStyleSheet("background: transparent;");
    versionListLayout = new QVBoxLayout(verListWrap);
    versionListLayout->setContentsMargins(0, 0, 0, 0);
    versionListLayout->setSpacing(2);
    versionListLayout->setAlignment(Qt::AlignTop);
    verScroll->setWidget(verListWrap);
    verL->addWidget(verScroll);

    QCheckBox* showAllChk = new QCheckBox("Показать все версии", verContent);
    showAllChk->setCursor(Qt::PointingHandCursor);
    showAllChk->setStyleSheet(QString(
        "QCheckBox { color: %1; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %2;"
        " border-radius: 4px; background: %3; }"
        "QCheckBox::indicator:checked { background: %4; border: 1px solid %4; }")
        .arg(kTextDim, kBorder, kBg, kAccent));
    connect(showAllChk, &QCheckBox::toggled, this, [this](bool on) {
        showAllVersions = on;
        rebuildVersionList();
    });
    verL->addWidget(showAllChk);

    connect(versionSearch, &QLineEdit::textChanged, this, [this]() {
        rebuildVersionList();
    });

    // ── Загрузчик: список с иконками + «Показать ещё» ───────────────────────────
    QWidget* loaderContentW = new QWidget();
    loaderListLayout = new QVBoxLayout(loaderContentW);
    loaderListLayout->setContentsMargins(10, 0, 10, 12);
    loaderListLayout->setSpacing(2);
    loaderListLayout->setAlignment(Qt::AlignTop);
    rebuildLoaderList();

    // ── Категория: список с иконками ────────────────────────────────────────────
    QWidget* catContent = new QWidget();
    categoryListLayout = new QVBoxLayout(catContent);
    categoryListLayout->setContentsMargins(10, 0, 10, 12);
    categoryListLayout->setSpacing(2);
    buildCategoryList();

    // ── Окружение ───────────────────────────────────────────────────────────────
    QWidget* envContent = new QWidget();
    QVBoxLayout* envL = new QVBoxLayout(envContent);
    envL->setContentsMargins(10, 0, 10, 12);
    envL->setSpacing(2);

    clientButton = new QPushButton("Клиент", this);
    clientButton->setCheckable(true);
    clientButton->setCursor(Qt::PointingHandCursor);
    clientButton->setStyleSheet(rowStyle);
    connect(clientButton, &QPushButton::clicked, this, [this]() {
        selectEnvironment(selectedEnvironment == "client" ? QString() : QString("client"));
    });

    serverButton = new QPushButton("Сервер", this);
    serverButton->setCheckable(true);
    serverButton->setCursor(Qt::PointingHandCursor);
    serverButton->setStyleSheet(rowStyle);
    connect(serverButton, &QPushButton::clicked, this, [this]() {
        selectEnvironment(selectedEnvironment == "server" ? QString() : QString("server"));
    });

    envL->addWidget(clientButton);
    envL->addWidget(serverButton);

    // ── Секции-аккордеоны ──────────────────────────────────────────────────────
    side->addWidget(makeSection("Версия игры", verContent,       &versionSummary,     &versionContent,     false));
    side->addWidget(makeSection("Загрузчик",   loaderContentW,   &loaderSummary,      &loaderContent,      false));
    side->addWidget(makeSection("Категория",   catContent,       &categorySummary,    &categoryContent,    false));
    side->addWidget(makeSection("Окружение",   envContent,       &environmentSummary, &environmentContent, false));
    side->addStretch(1);

    sideScroll->setWidget(panel);
    return sideScroll;
}

// Создаёт сворачиваемую секцию (плоский стиль с разделителем снизу): клик по
// заголовку открывает/закрывает контент, шеврон справа (▴ открыто / ▾ закрыто).
QWidget* ModWindow::makeSection(const QString& title, QWidget* content,
                                QLabel** summaryOut, QWidget** contentOut, bool expanded)
{
    const QString chevUp   = QString::fromUtf8("\u25B4"); // ▴
    const QString chevDown = QString::fromUtf8("\u25BE"); // ▾

    QWidget* container = new QWidget(this);
    container->setObjectName("section");
    container->setStyleSheet(QString(
        "QWidget#section { background: transparent; border-bottom: 1px solid %1; }")
        .arg(kBorder));

    QVBoxLayout* v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Заголовок: кликабельный QFrame с layout (надёжно рендерится).
    ClickableFrame* header = new ClickableFrame(container);
    header->setCursor(Qt::PointingHandCursor);
    header->setStyleSheet("QFrame:hover { background: transparent; }");
    QHBoxLayout* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 12, 14, 12);
    hl->setSpacing(8);

    QLabel* titleLbl = new QLabel(title, header);
    titleLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLbl->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;"
                                    " background: transparent;").arg(kText));

    QLabel* chev = new QLabel(expanded ? chevUp : chevDown, header);
    chev->setAttribute(Qt::WA_TransparentForMouseEvents);
    chev->setStyleSheet(QString("color: %1; background: transparent;").arg(kTextDim));

    hl->addWidget(titleLbl);
    hl->addStretch(1);
    hl->addWidget(chev);

    // Чип-сводка активного выбора (виден, только когда секция свёрнута и есть значение)
    QWidget* summaryWrap = new QWidget(container);
    QHBoxLayout* sw = new QHBoxLayout(summaryWrap);
    sw->setContentsMargins(14, 0, 14, 12);
    sw->setSpacing(6);

    QLabel* summary = new QLabel(summaryWrap);
    summary->setStyleSheet(QString("background-color: %1; color: %2; border-radius: 8px;"
                                   " padding: 3px 10px;").arg(kPanelHi, kText));
    sw->addWidget(summary);
    sw->addStretch(1);
    summaryWrap->setVisible(false);

    content->setParent(container);
    content->setVisible(expanded);

    v->addWidget(header);
    v->addWidget(summaryWrap);
    v->addWidget(content);

    header->onClick = [this, content, chev, chevUp, chevDown]() {
        bool now = !content->isVisible();
        content->setVisible(now);
        chev->setText(now ? chevUp : chevDown);
        refreshSummaryVisibility();
    };

    if (summaryOut)  *summaryOut  = summary;
    if (contentOut)  *contentOut  = content;
    return container;
}

// Обновляет тексты чипов-сводок под заголовками секций по текущим фильтрам.
void ModWindow::updateSidebarSummaries()
{
    auto setText = [](QLabel* lbl, const QString& text) {
        if (lbl) lbl->setText(text);
    };

    setText(versionSummary, currentVersion());

    QString loader = selectedLoader;
    if (!loader.isEmpty())
        loader[0] = loader[0].toUpper();
    setText(loaderSummary, loader);

    setText(categorySummary,
            selectedCategory.isEmpty() ? QString() : prettyCategory(selectedCategory));

    setText(environmentSummary,
            selectedEnvironment.isEmpty()
                ? QString()
                : (selectedEnvironment == "server" ? "Сервер" : "Клиент"));

    refreshSummaryVisibility();
}

// Чип-сводка видна, только когда секция свёрнута и есть выбранное значение.
void ModWindow::refreshSummaryVisibility()
{
    auto upd = [](QLabel* lbl, QWidget* content) {
        if (!lbl) return;
        QWidget* wrap = lbl->parentWidget();
        if (!wrap) return;
        const bool collapsed = (content && !content->isVisible());
        wrap->setVisible(collapsed && !lbl->text().isEmpty());
    };

    upd(versionSummary,     versionContent);
    upd(loaderSummary,      loaderContent);
    upd(categorySummary,    categoryContent);
    upd(environmentSummary, environmentContent);
}

// ── Рендер SVG-иконки Modrinth в QIcon нужного цвета ─────────────────────────
QIcon ModWindow::makeSvgIcon(const QString& svg, const QString& colorHex, int px)
{
    if (svg.trimmed().isEmpty())
        return QIcon();

    // Иконки Modrinth используют currentColor (stroke/fill) — перекрашиваем под тему.
    QString markup = svg;
    markup.replace("currentColor", colorHex);

    QSvgRenderer renderer(markup.toUtf8());
    if (!renderer.isValid())
        return QIcon();

    const qreal dpr = 2.0;
    QPixmap pix(int(px * dpr), int(px * dpr));
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    renderer.render(&p);
    p.end();
    pix.setDevicePixelRatio(dpr);
    return QIcon(pix);
}

void ModWindow::buildCategoryList()
{
    // Реальные категории модов из Modrinth (/v2/tag/category)
    static const QStringList cats = {
        "adventure", "cursed", "decoration", "economy", "equipment", "food",
        "game-mechanics", "library", "magic", "management", "minigame", "mobs",
        "optimization", "social", "storage", "technology", "transportation",
        "utility", "worldgen"
    };

    QString itemStyle = QString(
        "QPushButton { text-align: left; padding: 6px 8px; border: none;"
        " border-radius: 8px; color: %1; background: transparent; }"
        "QPushButton:hover { background-color: %2; color: %3; }"
        "QPushButton:checked { background-color: %4; color: #0A0A0A; font-weight: bold; }")
        .arg(kTextDim, kPanelHi, kText, kAccent);

    for (const QString& slug : cats)
    {
        QPushButton* btn = new QPushButton(prettyCategory(slug), this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(itemStyle);
        btn->setIconSize(QSize(18, 18));
        btn->setProperty("slug", slug);

        connect(btn, &QPushButton::clicked, this, [this, slug]() {
            selectCategory(selectedCategory == slug ? QString() : slug);
        });

        categoryButtons.push_back(btn);
        categoryListLayout->addWidget(btn);
    }

    updateCategoryIcons();
}

// ── Список версий (фильтр по поиску и «только релизы») ───────────────────────
void ModWindow::rebuildVersionList()
{
    if (!versionListLayout)
        return;

    // Чистим текущие кнопки
    QLayoutItem* it;
    while ((it = versionListLayout->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    versionButtons.clear();

    const QString rowStyle = QString(
        "QPushButton { text-align: left; padding: 6px 10px; border: none;"
        " border-radius: 8px; color: %1; background: transparent; }"
        "QPushButton:hover { background-color: %2; color: %3; }"
        "QPushButton:checked { background-color: %4; color: #0A0A0A; font-weight: bold; }")
        .arg(kTextDim, kPanelHi, kText, kAccent);

    const QString query = versionSearch ? versionSearch->text().trimmed() : QString();
    static const QRegularExpression releaseRe("^[0-9]+\\.[0-9]+(\\.[0-9]+)?$");

    for (const QString& v : allVersions)
    {
        if (!showAllVersions && !releaseRe.match(v).hasMatch())
            continue;
        if (!query.isEmpty() && !v.contains(query, Qt::CaseInsensitive))
            continue;

        QPushButton* btn = new QPushButton(v);
        btn->setCheckable(true);
        btn->setChecked(v == selectedVersion);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(rowStyle);
        connect(btn, &QPushButton::clicked, this, [this, v]() {
            selectVersion(selectedVersion == v ? QString() : v);
        });
        versionButtons.push_back(btn);
        versionListLayout->addWidget(btn);
    }
}

// ── Список загрузчиков (иконки + зелёная «таблетка» + «Показать ещё») ─────────
void ModWindow::rebuildLoaderList()
{
    if (!loaderListLayout)
        return;

    QLayoutItem* it;
    while ((it = loaderListLayout->takeAt(0)) != nullptr) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    loaderButtons.clear();

    const QString rowStyle = QString(
        "QPushButton { text-align: left; padding: 7px 10px; border: none;"
        " border-radius: 8px; color: %1; background: transparent; }"
        "QPushButton:hover { background-color: %2; color: %3; }"
        "QPushButton:checked { background-color: %4; color: #0A0A0A; font-weight: bold; }")
        .arg(kTextDim, kPanelHi, kText, kAccent);

    // Имена загрузчиков: из API, иначе — короткий запасной набор.
    QStringList names;
    if (!loaderTags.isEmpty())
        for (const TagInfo& t : loaderTags) names << t.name;
    else
        names = {"fabric", "forge", "neoforge", "quilt"};

    const int limit = 6;
    const int total = names.size();
    const int shownCount = showAllLoaders ? total : qMin(limit, total);

    for (int i = 0; i < shownCount; ++i)
    {
        const QString slug = names.at(i);
        QPushButton* btn = new QPushButton(prettyCategory(slug));
        btn->setCheckable(true);
        btn->setChecked(slug.toLower() == selectedLoader);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(rowStyle);
        btn->setIconSize(QSize(18, 18));
        btn->setProperty("slug", slug.toLower());
        if (loaderIcons.contains(slug))
            btn->setIcon(makeSvgIcon(loaderIcons.value(slug), kTextDim));

        connect(btn, &QPushButton::clicked, this, [this, slug]() {
            const QString l = slug.toLower();
            selectLoader(selectedLoader == l ? QString() : l);
        });
        loaderButtons.push_back(btn);
        loaderListLayout->addWidget(btn);
    }

    if (total > limit)
    {
        QPushButton* more = new QPushButton(
            showAllLoaders ? QString("Свернуть")
                           : QString("Показать ещё (%1)").arg(total - limit));
        more->setCursor(Qt::PointingHandCursor);
        more->setStyleSheet(QString(
            "QPushButton { text-align: left; padding: 6px 10px; border: none;"
            " background: transparent; color: %1; }"
            "QPushButton:hover { color: %2; }").arg(kTextDim, kAccent));
        connect(more, &QPushButton::clicked, this, [this]() {
            showAllLoaders = !showAllLoaders;
            rebuildLoaderList();
        });
        loaderListLayout->addWidget(more);
    }
}

void ModWindow::selectVersion(const QString& version)
{
    selectedVersion = version;
    for (QPushButton* b : versionButtons)
        b->setChecked(b->text() == version);

    rebuildActiveFilters();
    applyFilters();
}

void ModWindow::selectLoader(const QString& loader)
{
    selectedLoader = loader;   // нижний регистр или пусто
    for (QPushButton* b : loaderButtons)
        b->setChecked(b->property("slug").toString() == loader);

    rebuildActiveFilters();
    // Список версий зависит от загрузчика; обновление само вызовет applyFilters().
    updateVersionsForLoader();
}

void ModWindow::onCategoriesReceived(const QVector<TagInfo>& categories)
{
    for (const TagInfo& t : categories)
        categoryIcons.insert(t.name, t.icon);
    updateCategoryIcons();
}

void ModWindow::onLoadersReceived(const QVector<TagInfo>& loaders)
{
    loaderTags = loaders;
    loaderIcons.clear();
    for (const TagInfo& t : loaders)
        loaderIcons.insert(t.name, t.icon);
    rebuildLoaderList();
}

void ModWindow::updateCategoryIcons()
{
    for (QPushButton* b : categoryButtons)
    {
        const QString slug = b->property("slug").toString();
        if (categoryIcons.contains(slug))
            b->setIcon(makeSvgIcon(categoryIcons.value(slug), kTextDim));
    }
}

void ModWindow::updateLoaderIcons()
{
    for (QPushButton* b : loaderButtons)
    {
        const QString slug = b->property("slug").toString();
        if (loaderIcons.contains(slug))
            b->setIcon(makeSvgIcon(loaderIcons.value(slug), kTextDim));
    }
}

void ModWindow::selectCategory(const QString& category)
{
    selectedCategory = category;
    for (QPushButton* b : categoryButtons)
        b->setChecked(b->property("slug").toString() == category);

    rebuildActiveFilters();
    applyFilters();
}

void ModWindow::selectEnvironment(const QString& environment)
{
    selectedEnvironment = environment;
    if (clientButton) clientButton->setChecked(environment == "client");
    if (serverButton) serverButton->setChecked(environment == "server");

    rebuildActiveFilters();
    applyFilters();
}

// ===================== ACTIVE FILTER CHIPS =====================
void ModWindow::rebuildActiveFilters()
{
    // Очищаем полосу
    QLayoutItem* item;
    while ((item = chipsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    updateSidebarSummaries();

    const QString version = currentVersion();
    const QString loader  = currentLoader();

    const bool anyActive = !version.isEmpty() || !loader.isEmpty()
                         || !selectedCategory.isEmpty() || !selectedEnvironment.isEmpty();

    chipsBar->setVisible(anyActive);
    if (!anyActive)
        return;

    // Кнопка "Сбросить фильтры"
    QPushButton* clearAll = new QPushButton("Сбросить фильтры", chipsBar);
    clearAll->setCursor(Qt::PointingHandCursor);
    clearAll->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; padding: 4px 6px; }"
        "QPushButton:hover { color: %2; }").arg(kTextDim, kAccent));
    connect(clearAll, &QPushButton::clicked, this, &ModWindow::clearAllFilters);
    chipsLayout->addWidget(clearAll);

    auto addChip = [&](const QString& text, std::function<void()> onRemove) {
        QPushButton* chip = new QPushButton("✕  " + text, chipsBar);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(QString(
            "QPushButton { background-color: %1; color: %2; border: 1px solid %3;"
            " border-radius: 12px; padding: 4px 10px; }"
            "QPushButton:hover { border: 1px solid %4; color: %4; }")
            .arg(kPanel, kText, kBorder, kAccent));
        connect(chip, &QPushButton::clicked, this, onRemove);
        chipsLayout->addWidget(chip);
    };

    if (!version.isEmpty())
        addChip(version, [this]() { selectVersion(QString()); });

    if (!loader.isEmpty())
    {
        QString loaderLabel = loader;
        loaderLabel[0] = loaderLabel[0].toUpper();
        addChip(loaderLabel, [this]() { selectLoader(QString()); });
    }

    if (!selectedCategory.isEmpty())
        addChip(prettyCategory(selectedCategory), [this]() { selectCategory(QString()); });

    if (!selectedEnvironment.isEmpty())
        addChip(selectedEnvironment == "server" ? "Сервер" : "Клиент",
                [this]() { selectEnvironment(QString()); });

    chipsLayout->addStretch(1);
}

void ModWindow::clearAllFilters()
{
    selectedCategory.clear();
    selectedEnvironment.clear();
    selectedVersion.clear();
    selectedLoader.clear();

    for (QPushButton* b : categoryButtons)
        b->setChecked(false);
    if (clientButton) clientButton->setChecked(false);
    if (serverButton) serverButton->setChecked(false);
    rebuildLoaderList();

    // Сброс загрузчика вернёт список версий и сам вызовет applyFilters.
    updateVersionsForLoader();
    rebuildActiveFilters();
}

QString ModWindow::currentVersion() const
{
    return selectedVersion;
}

QString ModWindow::currentLoader() const
{
    return selectedLoader;
}

// ===================== VERSIONS =====================
void ModWindow::updateVersionsForLoader()
{
    const QString loader = currentLoader();

    if (loader.isEmpty())
    {
        // Без загрузчика — стандартный набор популярных релизов.
        allVersions = {"1.21.1","1.21","1.20.4","1.20.2","1.20.1","1.20",
                       "1.19.4","1.19.2","1.18.2","1.17.1","1.16.5","1.12.2","1.8.9"};
        rebuildVersionList();
        rebuildActiveFilters();
        applyFilters();
        return;
    }

    api->fetchAvailableVersions(loader);
}

void ModWindow::onAvailableVersions(const QString& loader, const QStringList& versions)
{
    Q_UNUSED(loader);
    allVersions = versions;
    rebuildVersionList();
    rebuildActiveFilters();
    applyFilters();
}

// ===================== APPLY FILTERS =====================
void ModWindow::applyFilters()
{
    QString query = searchEdit->text().trimmed();
    if (query.isEmpty()) query = "minecraft";

    QString version = currentVersion();
    QString loader = currentLoader();

    SortOrder order = static_cast<SortOrder>(sortCombo ? sortCombo->currentIndex() : 1);
    int count = viewCombo ? viewCombo->currentText().toInt() : 20;
    if (count <= 0) count = 20;

    rebuildActiveFilters();
    clearCards();
    api->getMods(query, version, loader, selectedCategory, selectedEnvironment, order, 0, count);
}

// ===================== CARDS =====================
void ModWindow::addModCards(const QVector<Mod>& mods)
{
    clearCards();

    if (mods.isEmpty()) {
        QLabel* empty = new QLabel("Ничего не найдено для выбранных фильтров");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("font-size: 16px; padding: 40px; color: %1;").arg(kTextDim));
        cardsLayout->addWidget(empty);
        return;
    }

    for (const Mod& mod : mods)
    {
        QFrame* card = new QFrame();
        card->setFrameShape(QFrame::StyledPanel);
        card->setCursor(Qt::PointingHandCursor);
        card->setMinimumHeight(120);
        card->setStyleSheet(QString(
            "QFrame { background-color: %1; border-radius: 10px; border: 1px solid %2; }"
            "QFrame:hover { border: 1px solid %3; }").arg(kPanel, kBorder, kAccent));
        card->setProperty("modId", mod.id);

        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(14, 14, 14, 14);
        cardLayout->setSpacing(14);

        // Иконка
        QLabel* iconLabel = new QLabel();
        iconLabel->setFixedSize(80, 80);
        iconLabel->setScaledContents(true);
        iconLabel->setStyleSheet("border-radius: 8px; border: none;");

        if (!mod.iconURL.isEmpty()) {
            QNetworkReply* reply = api->manager.get(QNetworkRequest(mod.iconURL));
            connect(reply, &QNetworkReply::finished, iconLabel, [iconLabel, reply]() {
                QPixmap pix;
                if (pix.loadFromData(reply->readAll())) {
                    iconLabel->setPixmap(pix.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                reply->deleteLater();
            });
        }

        // Информация (центр)
        QVBoxLayout* infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(6);

        QLabel* title = new QLabel(QString("<b>%1</b>  <span style='color:%2;'>от %3</span>")
                                       .arg(mod.name.toHtmlEscaped(), kTextDim, mod.author.toHtmlEscaped()));
        title->setStyleSheet(QString("font-size: 16px; color: %1; border: none;").arg(kText));

        QLabel* desc = new QLabel(mod.description);
        desc->setWordWrap(true);
        desc->setStyleSheet(QString("color: %1; border: none;").arg(kTextDim));

        infoLayout->addWidget(title);
        infoLayout->addWidget(desc);

        // Теги категорий (без загрузчиков)
        QHBoxLayout* tagsLayout = new QHBoxLayout();
        tagsLayout->setSpacing(6);
        tagsLayout->setContentsMargins(0, 0, 0, 0);

        int shown = 0, hidden = 0;
        for (const QString& c : mod.categories)
        {
            if (isLoaderTag(c)) continue;
            if (shown < 3)
            {
                QLabel* tag = new QLabel(prettyCategory(c));
                tag->setStyleSheet(QString(
                    "background-color: %1; color: %2; border-radius: 8px;"
                    " padding: 2px 8px; border: none;").arg(kPanelHi, kTextDim));
                tagsLayout->addWidget(tag);
                shown++;
            }
            else hidden++;
        }
        if (hidden > 0)
        {
            QLabel* more = new QLabel(QString("+%1").arg(hidden));
            more->setStyleSheet(QString(
                "background-color: %1; color: %2; border-radius: 8px;"
                " padding: 2px 8px; border: none;").arg(kPanelHi, kTextDim));
            tagsLayout->addWidget(more);
        }
        tagsLayout->addStretch(1);
        infoLayout->addLayout(tagsLayout);

        // Правая колонка: кнопка + статистика
        QVBoxLayout* rightLayout = new QVBoxLayout();
        rightLayout->setSpacing(6);

        QPushButton* installBtn = new QPushButton("+ Установить");
        installBtn->setFixedWidth(150);
        installBtn->setFixedHeight(38);
        installBtn->setCursor(Qt::PointingHandCursor);
        installBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; border: 1px solid %1;"
            " border-radius: 8px; font-weight: bold; }"
            "QPushButton:hover { background-color: %1; color: #0A0A0A; }").arg(kAccent));
        connect(installBtn, &QPushButton::clicked, this, [this, mod]() {
            installMod(mod);
        });

        QLabel* stats = new QLabel(QString("⬇ %1   ♥ %2")
                                       .arg(formatCount(mod.downloads), formatCount(mod.follows)));
        stats->setAlignment(Qt::AlignRight);
        stats->setStyleSheet(QString("color: %1; border: none;").arg(kTextDim));

        QString rel = formatRelativeDate(mod.dateUpdated);
        QLabel* date = new QLabel(rel.isEmpty() ? "" : ("🕒 " + rel));
        date->setAlignment(Qt::AlignRight);
        date->setStyleSheet(QString("color: %1; border: none;").arg(kTextDim));

        rightLayout->addWidget(installBtn, 0, Qt::AlignRight);
        rightLayout->addStretch(1);
        rightLayout->addWidget(stats);
        rightLayout->addWidget(date);

        cardLayout->addWidget(iconLabel, 0, Qt::AlignTop);
        cardLayout->addLayout(infoLayout, 1);
        cardLayout->addLayout(rightLayout);

        // Клик по карточке открывает страницу Modrinth
        card->installEventFilter(this);
        card->setProperty("modName", mod.name);

        cardsLayout->addWidget(card);
    }
}

void ModWindow::installMod(const Mod& mod)
{
    QString version = currentVersion();
    QString loader = currentLoader();

    api->getDownloadLinks(mod.id, version, loader);
}

void ModWindow::onDownloadLinks(const QVector<QUrl>& urls)
{
    if (urls.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не найдено подходящей версии мода для выбранных фильтров.");
        return;
    }

    QString gameDir = settingsWindow ? settingsWindow->minecraftPath() : "";
    if (gameDir.isEmpty())
        gameDir = QDir::homePath() + "/AppData/Roaming/.minecraft";

    QString modsPath = gameDir + "/mods";
    QDir().mkpath(modsPath);

    QUrl url = urls.first();
    QString fileName = QFileInfo(url.path()).fileName();
    if (fileName.isEmpty()) fileName = "mod.jar";

    QString savePath = modsPath + "/" + fileName;

    QNetworkReply* reply = api->manager.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, savePath, fileName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "Ошибка скачивания", reply->errorString());
            return;
        }

        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            QMessageBox::information(this, "Успех", "Мод успешно установлен!\n" + fileName);
        } else {
            QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл");
        }
    });
}

bool ModWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        QFrame* card = qobject_cast<QFrame*>(obj);

        if (card && mouseEvent)
        {
            QPoint pos = mouseEvent->pos();
            QWidget* child = card->childAt(pos);

            // Если клик был не по кнопке "Установить"
            if (!child || !qobject_cast<QPushButton*>(child))
            {
                QString slug = card->property("modId").toString();
                if (!slug.isEmpty())
                {
                    QUrl url("https://modrinth.com/mod/" + slug);
                    QDesktopServices::openUrl(url);
                }
            }
        }
    }

    return QDialog::eventFilter(obj, event);
}

void ModWindow::clearCards()
{
    QLayoutItem* item;
    while ((item = cardsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
}

// ===================== HELPERS =====================
QString ModWindow::formatCount(quint64 value)
{
    if (value >= 1000000ULL)
        return QString::number(value / 1000000.0, 'f', 2) + "M";
    if (value >= 1000ULL)
        return QString::number(value / 1000.0, 'f', 1) + "K";
    return QString::number(value);
}

QString ModWindow::formatRelativeDate(const QString& iso)
{
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return QString();

    qint64 days = dt.daysTo(QDateTime::currentDateTimeUtc());
    if (days <= 0) return "сегодня";
    if (days == 1) return "вчера";
    return QString("%1 дн. назад").arg(days);
}
