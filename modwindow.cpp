#include "modwindow.h"
#include "ModDetailsWindow.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

void ModWindow::onDownloadLinks(
    const QVector<QUrl>& urls)
{
    if(urls.isEmpty())
        return;

    QUrl downloadUrl = urls.first();

    QNetworkReply* reply =
        downloadManager.get(
            QNetworkRequest(downloadUrl));

    connect(reply,
            &QNetworkReply::finished,
            this,
            [this,
             reply,
             downloadUrl]()
            {
                reply->deleteLater();

                if(reply->error() !=
                    QNetworkReply::NoError)
                {
                    QMessageBox::warning(
                        this,
                        "Ошибка",
                        reply->errorString());

                    return;
                }

                QString modsPath =
                    QDir::homePath() +
                    "/AppData/Roaming/.minecraft/mods";

                QDir().mkpath(modsPath);

                QString fileName =
                    QFileInfo(
                        downloadUrl.path())
                        .fileName();

                QString filePath =
                    modsPath +
                    "/" +
                    fileName;

                QFile file(filePath);

                if(!file.open(
                        QIODevice::WriteOnly))
                {
                    QMessageBox::warning(
                        this,
                        "Ошибка",
                        "Не удалось создать файл");

                    return;
                }

                file.write(reply->readAll());
                file.close();

                QMessageBox::information(
                    this,
                    "Успех",
                    "Мод установлен:\n" +
                        fileName);
            });
}
ModWindow::ModWindow(QWidget *parent)
    : QDialog(parent)
{

    setWindowTitle("Менеджер модов");
    resize(900, 600);

    api = new ModrithAPI(this);

    auto* mainLayout = new QVBoxLayout(this);

    auto* searchLayout = new QHBoxLayout();

    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Введите название мода...");

    searchButton = new QPushButton("Поиск", this);

    searchLayout->addWidget(searchEdit);
    searchLayout->addWidget(searchButton);

    modList = new QListWidget(this);

    descriptionLabel = new QLabel(this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setMinimumHeight(100);

    installButton = new QPushButton("Скачать мод", this);

    mainLayout->addLayout(searchLayout);
    mainLayout->addWidget(modList);
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addWidget(installButton);
    connect(api,
            &ModrithAPI::DownloadLinks,
            this,
            &ModWindow::onDownloadLinks);
    connect(searchButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                modList->clear();

                api->getMods(
                    searchEdit->text(),
                    SortOrder::downloads,
                    0,
                    50);
            });

    connect(api,
            &ModrithAPI::ModList,
            this,
            [this](const QVector<Mod>& mods)
            {
                cachedMods = mods;

                modList->clear();

                for(const auto& mod : mods)
                {
                    modList->addItem(
                        mod.name +
                        " | " +
                        QString::number(mod.downloads));
                }
            });
    connect(modList,
            &QListWidget::itemDoubleClicked,
            this,
            [this](QListWidgetItem*)
            {
                int row =
                    modList->currentRow();

                if(row < 0 ||
                    row >= cachedMods.size())
                    return;

                ModDetailsWindow dlg(
                    cachedMods[row],
                    this);

                dlg.exec();
            });
    connect(modList,
            &QListWidget::currentRowChanged,
            this,
            [this](int row)
            {
                if(row < 0 ||
                    row >= cachedMods.size())
                    return;

                const Mod& mod =
                    cachedMods[row];

                descriptionLabel->setText(
                    "<b>" +
                    mod.name +
                    "</b><br><br>" +
                    mod.description);
            });

    connect(installButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                int row =
                    modList->currentRow();

                if(row < 0 ||
                    row >= cachedMods.size())
                    return;

                const Mod& mod =
                    cachedMods[row];

                QMessageBox::information(
                    this,
                    "Мод",
                    "Выбран мод:\n" +
                        mod.name);

                // позже:
                // api->getDownloadLinks(...)
            });

    connect(api,
            &ModrithAPI::OnError,
            this,
            [this](const QString& err)
            {
                QMessageBox::warning(
                    this,
                    "Ошибка",
                    err);
            });
}
