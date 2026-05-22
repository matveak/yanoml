#include "modwindow.h"
#include "ModDetailWindow.h"
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
#include <QEvent>
#include "settingswindow.h"

ModWindow::ModWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Менеджер модов — Modrinth");
    resize(1200, 820);
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
    versionFilter->setFixedWidth(160);

    loaderFilter = new QComboBox(this);
    loaderFilter->addItems({"Любой загрузчик", "fabric", "forge", "neoforge", "quilt"});
    loaderFilter->setFixedWidth(160);

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
    cardsLayout->setContentsMargins(0, 0, 0, 0);

    scrollArea->setWidget(cardsWidget);

    // Кнопка установки
    installButton = new QPushButton("Установить выбранный мод", this);
    installButton->setFixedHeight(52);

    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(scrollArea, 1);
    mainLayout->addWidget(installButton);

    // ==================== СИГНАЛЫ ====================
    connect(searchButton, &QPushButton::clicked, this, &ModWindow::applyFilters);
    connect(versionFilter, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);
    connect(loaderFilter, &QComboBox::currentIndexChanged, this, &ModWindow::applyFilters);

    connect(api, &ModrithAPI::ModList, this, &ModWindow::addModCards);
    connect(installButton, &QPushButton::clicked, this, &ModWindow::installSelectedMod);
    connect(api, &ModrithAPI::DownloadLinks, this, &ModWindow::onDownloadLinks);

    // Начальный поиск
    applyFilters();
}

void ModWindow::applyFilters()
{
    QString query =
        searchEdit->text().trimmed();

    if(query.isEmpty())
        query = "minecraft";

    QString version;
    QString loader;

    if(versionFilter &&
        versionFilter->currentText() !=
            "Любая версия")
    {
        version =
            versionFilter->currentText();
    }

    if(loaderFilter &&
        loaderFilter->currentText() !=
            "Любой загрузчик")
    {
        loader =
            loaderFilter->currentText()
                .toLower();
    }

    clearCards();

    api->getMods(
        query,
        version,
        loader,
        SortOrder::downloads,
        0,
        30);
}

void ModWindow::clearCards()
{
    QLayoutItem* item;
    while ((item = cardsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

void ModWindow::addModCards(const QList<Mod>& mods)
{
    clearCards();
    cachedMods.clear();

    QString version =
        versionFilter->currentText();

    QString loader =
        loaderFilter->currentText();

    for(const Mod& mod : mods)
    {
        // ==========================
        // VERSION FILTER
        // ==========================

        if(version != "Любая версия")
        {
            if(!mod.versions.contains(version))
                continue;
        }

        // ==========================
        // LOADER FILTER
        // ==========================

        if(loader != "Любой загрузчик")
        {
            bool found = false;

            for(const QString& cat : mod.categories)
            {
                if(cat.compare(
                        loader,
                        Qt::CaseInsensitive) == 0)
                {
                    found = true;
                    break;
                }
            }

            if(!found)
                continue;
        }

        cachedMods.push_back(mod);

        QFrame* card = new QFrame();

        card->setFrameShape(
            QFrame::StyledPanel);

        card->setCursor(
            Qt::PointingHandCursor);

        card->setFixedHeight(110);

        card->setProperty(
            "modId",
            mod.id);

        card->installEventFilter(this);

        QHBoxLayout* cardLayout =
            new QHBoxLayout(card);

        QLabel* iconLabel =
            new QLabel();

        iconLabel->setFixedSize(
            72,
            72);

        iconLabel->setScaledContents(true);

        if(!mod.iconURL.isEmpty())
        {
            QNetworkReply* reply =
                api->manager.get(
                    QNetworkRequest(
                        mod.iconURL));

            connect(
                reply,
                &QNetworkReply::finished,
                iconLabel,
                [iconLabel, reply]()
                {
                    QPixmap pix;

                    if(pix.loadFromData(
                            reply->readAll()))
                    {
                        iconLabel->setPixmap(
                            pix.scaled(
                                72,
                                72,
                                Qt::KeepAspectRatio,
                                Qt::SmoothTransformation));
                    }

                    reply->deleteLater();
                });
        }

        QVBoxLayout* info =
            new QVBoxLayout();

        QLabel* name =
            new QLabel(
                "<b>" +
                mod.name +
                "</b>");

        QLabel* desc =
            new QLabel(
                mod.description);

        desc->setWordWrap(true);

        QLabel* stats =
            new QLabel(
                QString("↓ %1")
                    .arg(mod.downloads));

        info->addWidget(name);
        info->addWidget(desc);
        info->addWidget(stats);

        cardLayout->addWidget(iconLabel);
        cardLayout->addLayout(info);

        cardsLayout->addWidget(card);
    }

    if(cachedMods.isEmpty())
    {
        QLabel* empty =
            new QLabel(
                "Ничего не найдено");

        empty->setAlignment(
            Qt::AlignCenter);

        cardsLayout->addWidget(empty);
    }
}
bool ModWindow::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress)
    {
        if(QFrame* card = qobject_cast<QFrame*>(obj))
        {
            selectedModId =
                card->property("modId").toString();

            for(const auto& mod : cachedMods)
            {
                if(mod.id == selectedModId)
                {
                    ModDetailsWindow dlg(mod, this);
                    dlg.exec();
                    break;
                }
            }

            return true;
        }
    }

    return QDialog::eventFilter(obj, event);
}

void ModWindow::installSelectedMod()
{
    if(selectedModId.isEmpty())
    {
        QMessageBox::warning(
            this,
            "Ошибка",
            "Сначала выберите мод");
        return;
    }

    QString loader;

    if(loaderFilter->currentText() !=
        "Любой загрузчик")
    {
        loader =
            loaderFilter->currentText();
    }

    api->getDownloadLinks(
        selectedModId,
        minecraftVersion,
        loader);
}

void ModWindow::onDownloadLinks(const QVector<QUrl>& urls)
{
    if (urls.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не найдено файлов для скачивания");
        return;
    }

    QUrl url = urls.first();

    QString modsPath;
    if (settingsWindow && !settingsWindow->minecraftPath().isEmpty()) {
        modsPath = settingsWindow->minecraftPath() + "/mods";
    } else {
        modsPath = QDir::homePath() + "/AppData/Roaming/.minecraft/mods";
    }

    QDir().mkpath(modsPath);

    QString fileName = QFileInfo(url.path()).fileName();
    if (fileName.isEmpty() || fileName == "/") {
        fileName = "mod_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".jar";
    }

    QString savePath = modsPath + "/" + fileName;

    qDebug() << "Скачиваем мод в:" << savePath;

    QNetworkReply* reply = api->manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "Ошибка скачивания", reply->errorString());
            return;
        }

        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            QMessageBox::information(this, "Успех",
                                     "Мод успешно установлен!\n\nФайл: " + fileName);
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось сохранить файл:\n" + savePath);
        }
    });
}

void ModWindow::setMinecraftVersion(const QString& version)
{
    minecraftVersion = version;
}