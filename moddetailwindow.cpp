#include "moddetailwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QScrollArea>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <QPixmap>
#include <QDebug>

ModDetailsWindow::ModDetailsWindow(
    const Mod& mod,
    QWidget* parent)
    : QDialog(parent),
    currentMod(mod)
{
    resize(1000, 750);

    setWindowTitle(mod.name);

    api = new ModrithAPI(this);
    manager = new QNetworkAccessManager(this);

    QVBoxLayout* mainLayout =
        new QVBoxLayout(this);

    // =========================
    // HEADER
    // =========================

    QHBoxLayout* headerLayout =
        new QHBoxLayout();

    iconLabel =
        new QLabel(this);

    iconLabel->setFixedSize(128, 128);
    iconLabel->setScaledContents(true);

    QVBoxLayout* infoLayout =
        new QVBoxLayout();

    titleLabel =
        new QLabel(
            "<h1>" + mod.name + "</h1>");

    authorLabel =
        new QLabel(
            "Автор: " + mod.author);

    downloadsLabel =
        new QLabel(
            "Загрузок: " +
            QString::number(mod.downloads));

    categoriesLabel =
        new QLabel();

    versionsLabel =
        new QLabel(
            "Версии: " +
            mod.versions.join(", "));

    infoLayout->addWidget(titleLabel);
    infoLayout->addWidget(authorLabel);
    infoLayout->addWidget(downloadsLabel);
    infoLayout->addWidget(categoriesLabel);
    infoLayout->addWidget(versionsLabel);

    headerLayout->addWidget(iconLabel);
    headerLayout->addLayout(infoLayout);

    // =========================
    // DESCRIPTION
    // =========================

    descriptionBrowser =
        new QTextBrowser(this);

    descriptionBrowser->setOpenExternalLinks(true);
    descriptionBrowser->setText(
        "Загрузка описания...");

    // =========================
    // GALLERY
    // =========================

    QScrollArea* galleryArea =
        new QScrollArea(this);

    galleryArea->setWidgetResizable(true);

    QWidget* galleryWidget =
        new QWidget();

    galleryLayout =
        new QVBoxLayout(galleryWidget);

    galleryArea->setWidget(galleryWidget);

    // =========================
    // INSTALL BUTTON
    // =========================

    installButton =
        new QPushButton(
            "Установить мод",
            this);

    // =========================
    // ADD TO LAYOUT
    // =========================

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(descriptionBrowser, 1);
    mainLayout->addWidget(galleryArea, 1);
    mainLayout->addWidget(installButton);

    // =========================
    // SIGNALS
    // =========================

    connect(
        api,
        &ModrithAPI::ProjectReceived,
        this,
        &ModDetailsWindow::onProjectReceived);

    connect(
        api,
        &ModrithAPI::OnError,
        this,
        [](const QString& error)
        {
            qDebug() << error;
        });

    api->getProject(
        currentMod.id);
}

void ModDetailsWindow::onProjectReceived(
    const ModProject& project)
{
    titleLabel->setText(
        "<h1>" +
        project.title +
        "</h1>");

    authorLabel->setText(
        "Автор: " +
        project.author);

    downloadsLabel->setText(
        "Загрузок: " +
        QString::number(project.downloads));

    categoriesLabel->setText(
        "Категории: " +
        project.categories.join(", "));

#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
    descriptionBrowser->setMarkdown(
        project.body);
#else
    descriptionBrowser->setPlainText(
        project.body);
#endif

    // =========================
    // ICON
    // =========================

    if(!project.iconUrl.isEmpty())
    {
        QNetworkReply* reply =
            manager->get(
                QNetworkRequest(
                    QUrl(project.iconUrl)));

        connect(
            reply,
            &QNetworkReply::finished,
            this,
            [this, reply]()
            {
                QByteArray data =
                    reply->readAll();

                QPixmap pix;

                if(pix.loadFromData(data))
                {
                    iconLabel->setPixmap(
                        pix.scaled(
                            128,
                            128,
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation));
                }

                reply->deleteLater();
            });
    }

    // =========================
    // GALLERY
    // =========================

    for(const QString& imageUrl :
         project.gallery)
    {
        QLabel* imageLabel =
            new QLabel();

        imageLabel->setAlignment(
            Qt::AlignCenter);

        imageLabel->setMinimumHeight(
            250);

        imageLabel->setText(
            "Загрузка изображения...");

        galleryLayout->addWidget(
            imageLabel);

        QNetworkReply* reply =
            manager->get(
                QNetworkRequest(
                    QUrl(imageUrl)));

        connect(
            reply,
            &QNetworkReply::finished,
            this,
            [imageLabel, reply]()
            {
                QByteArray data =
                    reply->readAll();

                QPixmap pix;

                if(pix.loadFromData(data))
                {
                    imageLabel->setPixmap(
                        pix.scaledToWidth(
                            850,
                            Qt::SmoothTransformation));
                }
                else
                {
                    imageLabel->setText(
                        "Не удалось загрузить изображение");
                }

                reply->deleteLater();
            });
    }
}