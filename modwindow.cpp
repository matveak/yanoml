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
#include <QNetworkReply>
#include <QDesktopServices>
#include <QMouseEvent>

ModWindow::ModWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Менеджер модов — Modrinth");
    resize(1350, 900);
    setStyleSheet("background-color: #18181B; color: #F1F1F1;");

    api = new ModrithAPI(this);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // Поисковая строка
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Поиск модов...");
    searchEdit->setFixedHeight(42);

    searchButton = new QPushButton("Поиск", this);
    searchButton->setFixedHeight(42);
    searchButton->setFixedWidth(140);

    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(searchButton);
    mainLayout->addLayout(searchLayout);

    // === ФИЛЬТРЫ ===
    setupAdvancedFilters();

    // Карточки модов
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    cardsWidget = new QWidget();
    cardsLayout = new QVBoxLayout(cardsWidget);
    cardsLayout->setSpacing(12);
    cardsLayout->setContentsMargins(8, 8, 8, 8);
    cardsLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidget(cardsWidget);
    mainLayout->addWidget(scrollArea, 1);

    // Сигналы
    connect(searchButton, &QPushButton::clicked, this, &ModWindow::applyFilters);
    connect(versionFilter, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);
    connect(loaderFilter, &QComboBox::currentTextChanged, this, &ModWindow::updateVersionsForLoader);
    connect(categoryFilter, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);
    connect(environmentFilter, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);

    connect(api, &ModrithAPI::AvailableVersions, this, &ModWindow::onAvailableVersions);
    connect(api, &ModrithAPI::ModList, this, &ModWindow::addModCards);
    connect(api, &ModrithAPI::DownloadLinks, this, &ModWindow::onDownloadLinks);
    connect(api, &ModrithAPI::OnError, this, [this](const QString& error){
        QMessageBox::warning(this, "Modrinth", error);
    });

    updateVersionsForLoader();
}

void ModWindow::setupAdvancedFilters()
{
    QHBoxLayout* filtersLayout = new QHBoxLayout();

    // Версия
    versionFilter = new QComboBox(this);
    versionFilter->setFixedWidth(160);

    // Загрузчик
    loaderFilter = new QComboBox(this);
    loaderFilter->addItems({"Любой загрузчик", "fabric", "forge", "neoforge", "quilt"});
    loaderFilter->setFixedWidth(160);

    // Категория
    categoryFilter = new QComboBox(this);
    categoryFilter->addItems({"Любая категория", "adventure", "optimization", "technology",
                              "magic", "utility", "library", "decoration", "equipment",
                              "food", "game-mechanics", "mobs", "storage", "worldgen",
                              "social", "management", "transportation"});
    categoryFilter->setFixedWidth(180);

    // Окружение
    environmentFilter = new QComboBox(this);
    environmentFilter->addItems({"Любая среда", "client", "server"});
    environmentFilter->setFixedWidth(140);

    filtersLayout->addWidget(new QLabel("Версия:"));
    filtersLayout->addWidget(versionFilter);
    filtersLayout->addWidget(new QLabel("Загрузчик:"));
    filtersLayout->addWidget(loaderFilter);
    filtersLayout->addWidget(new QLabel("Категория:"));
    filtersLayout->addWidget(categoryFilter);
    filtersLayout->addWidget(new QLabel("Окружение:"));
    filtersLayout->addWidget(environmentFilter);

    QVBoxLayout* main = qobject_cast<QVBoxLayout*>(layout());
    if (main)
        main->insertLayout(1, filtersLayout);
}

void ModWindow::updateVersionsForLoader()
{
    QString loaderText = loaderFilter->currentText();
    QString loader = (loaderText == "Любой загрузчик") ? "" : loaderText.toLower();

    versionFilter->clear();
    versionFilter->addItem("Загрузка версий...");

    if (loader.isEmpty())
    {
        versionFilter->clear();
        versionFilter->addItem("Любая версия");
        versionFilter->addItems({"1.21.1","1.21","1.20.1","1.20.2","1.20","1.19.2","1.18.2","1.17.1","1.16.5","1.12.2"});
        applyFilters();
        return;
    }

    // Запрашиваем версии
    api->fetchAvailableVersions(loader);
}

void ModWindow::onAvailableVersions(const QString& loader, const QStringList& versions)
{
    versionFilter->clear();
    versionFilter->addItem("Любая версия");

    for (const QString& v : versions)
        versionFilter->addItem(v);

    applyFilters();
}

void ModWindow::applyFilters()
{
    QString query = searchEdit->text().trimmed();
    if (query.isEmpty()) query = "minecraft";

    QString version = (versionFilter->currentText() == "Любая версия") ? "" : versionFilter->currentText();
    QString loader = (loaderFilter->currentText() == "Любой загрузчик") ? "" : loaderFilter->currentText().toLower();
    QString category = (categoryFilter->currentText() == "Любая категория") ? "" : categoryFilter->currentText().toLower();
    QString environment = (environmentFilter->currentText() == "Любая среда") ? "" : environmentFilter->currentText().toLower();

    clearCards();
    api->getMods(query, version, loader, category, environment, SortOrder::downloads, 0, 30);
}

void ModWindow::addModCards(const QVector<Mod>& mods)
{
    clearCards();

    if (mods.isEmpty()) {
        QLabel* empty = new QLabel("Ничего не найдено для выбранных фильтров");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("font-size: 16px; padding: 40px; color: #9CA3AF;");
        cardsLayout->addWidget(empty);
        return;
    }

    for (const Mod& mod : mods)
    {
        QFrame* card = new QFrame();
        card->setFrameShape(QFrame::StyledPanel);
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedHeight(130);
        card->setStyleSheet("QFrame { background-color: #27272A; border-radius: 8px; border: 1px solid #3F3F46; }");
        card->setProperty("modSlug", mod.id);

        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(12);

        // Иконка
        QLabel* iconLabel = new QLabel();
        iconLabel->setFixedSize(80, 80);
        iconLabel->setScaledContents(true);

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

        // Информация
        QVBoxLayout* infoLayout = new QVBoxLayout();
        QLabel* name = new QLabel("<b>" + mod.name + "</b>");
        name->setStyleSheet("font-size: 15px;");
        QLabel* desc = new QLabel(mod.description);
        desc->setWordWrap(true);
        desc->setMaximumWidth(680);
        desc->setStyleSheet("color: #A1A1AA;");

        infoLayout->addWidget(name);
        infoLayout->addWidget(desc);

        // Кнопка Установить
        QPushButton* installBtn = new QPushButton("Установить");
        installBtn->setFixedWidth(140);
        installBtn->setFixedHeight(50);
        installBtn->setStyleSheet("background-color: #22C55E; color: black; font-weight: bold;");

        // Клик по кнопке — только установка
        connect(installBtn, &QPushButton::clicked, this, [this, mod]() {
            installMod(mod);
        });

        cardLayout->addWidget(iconLabel);
        cardLayout->addLayout(infoLayout, 1);
        cardLayout->addWidget(installBtn);

        // === Клик по карточке (открытие страницы Modrinth) ===
        card->installEventFilter(this);  // Будем фильтровать события

        // Сохраняем данные мода в карточке
        card->setProperty("modId", mod.id);
        card->setProperty("modName", mod.name);

        cardsLayout->addWidget(card);
    }
}

void ModWindow::installMod(const Mod& mod)
{
    QString version = (versionFilter->currentText() == "Любая версия") ? "" : versionFilter->currentText();
    QString loader = (loaderFilter->currentText() == "Любой загрузчик") ? "" : loaderFilter->currentText().toLower();

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
        QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);  // используем dynamic_cast
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