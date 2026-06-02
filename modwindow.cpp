#include "modwindow.h"
#include "moddetailwindow.h"
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
#include <QDateTime>
#include "settingswindow.h"

ModWindow::ModWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Менеджер модов — Modrinth");
    resize(1250, 850);
    setStyleSheet("background-color: #18181B; color: #F1F1F1;");

    api = new ModrithAPI(this);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // ==================== ФИЛЬТРЫ ====================
    QHBoxLayout* filterLayout = new QHBoxLayout();

    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Поиск модов...");
    searchEdit->setFixedHeight(42);

    versionFilter = new QComboBox(this);
    versionFilter->addItems({"Любая версия", "1.21.1", "1.21", "1.20.1", "1.20", "1.19.2", "1.18.2"});
    versionFilter->setFixedWidth(170);

    loaderFilter = new QComboBox(this);
    loaderFilter->addItems({"Любой загрузчик", "fabric", "forge", "neoforge", "quilt"});
    loaderFilter->setFixedWidth(170);

    searchButton = new QPushButton("Поиск", this);
    searchButton->setFixedHeight(42);
    searchButton->setFixedWidth(140);

    filterLayout->addWidget(searchEdit, 1);
    filterLayout->addWidget(versionFilter);
    filterLayout->addWidget(loaderFilter);
    filterLayout->addWidget(searchButton);

    // ==================== КАРТОЧКИ ====================
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    cardsWidget = new QWidget();
    cardsLayout = new QVBoxLayout(cardsWidget);
    cardsLayout->setSpacing(12);
    cardsLayout->setContentsMargins(8, 8, 8, 8);

    scrollArea->setWidget(cardsWidget);

    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(scrollArea, 1);

    // ==================== СИГНАЛЫ ====================
    connect(searchButton, &QPushButton::clicked, this, &ModWindow::applyFilters);
    connect(versionFilter, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);
    connect(loaderFilter, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);

    connect(api, &ModrithAPI::ModList, this, &ModWindow::addModCards);
    connect(api, &ModrithAPI::DownloadLinks, this, &ModWindow::onDownloadLinks);

    applyFilters(); // начальная загрузка
}

void ModWindow::applyFilters()
{
    QString query = searchEdit->text().trimmed();
    if (query.isEmpty()) query = "minecraft";

    QString version = (versionFilter->currentText() == "Любая версия") ? "" : versionFilter->currentText();
    QString loader = (loaderFilter->currentText() == "Любой загрузчик") ? "" : loaderFilter->currentText().toLower();

    clearCards();
    api->getMods(query, version, loader, SortOrder::downloads, 0, 30);
}

void ModWindow::addModCards(const QList<Mod>& mods)
{
    clearCards();
    cachedMods = mods;

    for (const Mod& mod : mods)
    {
        QFrame* card = new QFrame();
        card->setFrameShape(QFrame::StyledPanel);
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedHeight(120);
        card->setProperty("modId", mod.id);

        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);

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
        QLabel* desc = new QLabel(mod.description);
        desc->setWordWrap(true);
        desc->setMaximumWidth(700);

        infoLayout->addWidget(name);
        infoLayout->addWidget(desc);

        // Кнопка
        QPushButton* installBtn = new QPushButton("Установить");
        installBtn->setFixedWidth(130);
        installBtn->setProperty("modId", mod.id);

        connect(installBtn, &QPushButton::clicked, this, [this, mod]() {
            installMod(mod);
        });

        cardLayout->addWidget(iconLabel);
        cardLayout->addLayout(infoLayout, 1);
        cardLayout->addWidget(installBtn);

        cardsLayout->addWidget(card);
    }

    if (mods.isEmpty()) {
        QLabel* empty = new QLabel("Ничего не найдено");
        empty->setAlignment(Qt::AlignCenter);
        cardsLayout->addWidget(empty);
    }
}

void ModWindow::installMod(const Mod& mod)
{
    QString version = versionFilter->currentText() == "Любая версия" ? "" : versionFilter->currentText();
    QString loader = loaderFilter->currentText() == "Любой загрузчик" ? "" : loaderFilter->currentText().toLower();

    api->getDownloadLinks(mod.id, version, loader);
}

void ModWindow::onDownloadLinks(const QVector<QUrl>& urls)
{
    if (urls.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не найдено ссылок для скачивания");
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
        }
    });
}

void ModWindow::clearCards()
{
    QLayoutItem* item;
    while ((item = cardsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}