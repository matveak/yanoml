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
#include <QStandardPaths>
#include <QFileInfo>

ModWindow::ModWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Менеджер модов — Modrinth");
    resize(1150, 780);
    setStyleSheet("background-color: #18181B; color: #F1F1F1;");

    api = new ModrithAPI(this);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // Поиск
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Поиск модов...");
    searchEdit->setFixedHeight(42);

    searchButton = new QPushButton("Поиск", this);
    searchButton->setFixedHeight(42);
    searchButton->setFixedWidth(120);

    searchLayout->addWidget(searchEdit);
    searchLayout->addWidget(searchButton);

    // Карточки
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    cardsWidget = new QWidget();
    cardsLayout = new QVBoxLayout(cardsWidget);
    cardsLayout->setSpacing(12);
    cardsLayout->setContentsMargins(0, 0, 0, 0);

    scrollArea->setWidget(cardsWidget);

    // Кнопка
    installButton = new QPushButton("Установить выбранный мод", this);
    installButton->setFixedHeight(52);

    mainLayout->addLayout(searchLayout);
    mainLayout->addWidget(scrollArea, 1);
    mainLayout->addWidget(installButton);

    // Сигналы
    connect(searchButton, &QPushButton::clicked, this, [this]() {
        QString query = searchEdit->text().trimmed();
        if (query.isEmpty()) query = "minecraft";
        clearCards();
        api->getMods(query, SortOrder::downloads, 0, 30);
    });

    connect(api, &ModrithAPI::ModList, this, &ModWindow::addModCards);
    connect(installButton, &QPushButton::clicked, this, &ModWindow::installSelectedMod);
    connect(api, &ModrithAPI::DownloadLinks, this, &ModWindow::onDownloadLinks);
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
    cachedMods = mods;
    clearCards();

    for (const auto& mod : mods) {
        QFrame* card = new QFrame();
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet(R"(
            QFrame {
                background-color: #27272A;
                border-radius: 10px;
                border: 1px solid #3F3F46;
            }
            QFrame:hover { border: 1px solid #60A5FA; }
        )");
        card->setFixedHeight(110);

        QHBoxLayout* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(12);

        QLabel* iconLabel = new QLabel();
        iconLabel->setFixedSize(72, 72);
        iconLabel->setScaledContents(true);

        if (!mod.iconURL.isEmpty()) {
            QNetworkReply* reply = api->manager.get(QNetworkRequest(mod.iconURL));
            connect(reply, &QNetworkReply::finished, iconLabel, [iconLabel, reply]() {
                QByteArray data = reply->readAll();
                QPixmap pix;
                if (pix.loadFromData(data)) {
                    iconLabel->setPixmap(pix.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                reply->deleteLater();
            });
        }

        QVBoxLayout* infoLayout = new QVBoxLayout();
        QLabel* nameLabel = new QLabel("<b>" + mod.name + "</b>");
        QLabel* descLabel = new QLabel(mod.description.left(150) + "...");
        descLabel->setWordWrap(true);
        QLabel* statsLabel = new QLabel(QString("↓ %1").arg(mod.downloads));

        infoLayout->addWidget(nameLabel);
        infoLayout->addWidget(descLabel);
        infoLayout->addWidget(statsLabel);

        cardLayout->addWidget(iconLabel);
        cardLayout->addLayout(infoLayout, 1);

        cardsLayout->addWidget(card);
        card->setProperty("modId", mod.id);
        card->installEventFilter(this);
    }
}

bool ModWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (QFrame* card = qobject_cast<QFrame*>(obj)) {
            // Здесь можно отметить выбранную карточку
            qDebug() << "Выбрана карточка:" << card->property("modName").toString();
        }
    }
    return QDialog::eventFilter(obj, event);
}

void ModWindow::installSelectedMod()
{
    if (cachedMods.isEmpty()) return;
    api->getDownloadLinks(cachedMods.first().id, "1.21.1", "");
}

void ModWindow::onDownloadLinks(const QVector<QUrl>& urls)
{
    if (urls.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не найдено файлов");
        return;
    }

    QUrl url = urls.first();
    QString modsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/.minecraft/mods";
    QDir().mkpath(modsPath);

    QString fileName = QFileInfo(url.path()).fileName();
    if (fileName.isEmpty()) fileName = "mod.jar";

    QString savePath = modsPath + "/" + fileName;

    QNetworkReply* reply = api->manager.get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "Ошибка", reply->errorString());
            return;
        }

        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            QMessageBox::information(this, "Успех", "Мод установлен:\n" + fileName);
        }
    });
}