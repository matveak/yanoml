#include "moddetailwindow.h"
#include "darktheme.h"

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

namespace {
const char* kBg      = DarkTheme::kBg;
const char* kPanel   = DarkTheme::kPanel;
const char* kBorder  = DarkTheme::kBorder;
const char* kText    = DarkTheme::kText;
const char* kTextDim = DarkTheme::kTextDim;
const char* kAccent  = DarkTheme::kAccent;
} // namespace

ModDetailsWindow::ModDetailsWindow(
    const Mod& mod,
    QWidget* parent)
    : QDialog(parent),
    currentMod(mod)
{
    resize(1000, 750);
    setWindowTitle(mod.name);
    setStyleSheet(DarkTheme::dialogStyle());

    api = new ModrithAPI(this);
    manager = new QNetworkAccessManager(this);

    QVBoxLayout* mainLayout =
        new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // =========================
    // HEADER
    // =========================

    QHBoxLayout* headerLayout =
        new QHBoxLayout();

    iconLabel =
        new QLabel(this);

    iconLabel->setFixedSize(128, 128);
    iconLabel->setScaledContents(true);
    iconLabel->setStyleSheet(QString(
                                 "border-radius: 12px; border: 1px solid %1; background: %2;")
                                 .arg(kBorder, kPanel));

    QVBoxLayout* infoLayout =
        new QVBoxLayout();

    infoLayout->setSpacing(6);

    titleLabel =
        new QLabel(
            "<h1>" + mod.name + "</h1>");
    titleLabel->setStyleSheet(QString("font-size: 26px; color: %1;").arg(kText));

    authorLabel =
        new QLabel(
            "Автор: " + mod.author);
    authorLabel->setStyleSheet(QString("color: %1;").arg(kTextDim));

    downloadsLabel =
        new QLabel(
            "⬇ " +
            QString::number(mod.downloads));
    downloadsLabel->setStyleSheet(QString("color: %1;").arg(kTextDim));

    categoriesLabel =
        new QLabel();
    categoriesLabel->setWordWrap(true);
    categoriesLabel->setStyleSheet(QString("color: %1;").arg(kTextDim));

    versionsLabel =
        new QLabel(
            "Версии: " +
            mod.versions.join(", "));
    versionsLabel->setWordWrap(true);
    versionsLabel->setStyleSheet(QString("color: %1;").arg(kTextDim));

    infoLayout->addWidget(titleLabel);
    infoLayout->addWidget(authorLabel);
    infoLayout->addWidget(downloadsLabel);
    infoLayout->addWidget(categoriesLabel);
    infoLayout->addWidget(versionsLabel);
    infoLayout->addStretch(1);

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    headerLayout->addSpacing(16);
    headerLayout->addLayout(infoLayout, 1);

    // =========================
    // DESCRIPTION
    // =========================

    descriptionBrowser =
        new QTextBrowser(this);

    descriptionBrowser->setOpenExternalLinks(true);
    descriptionBrowser->setText(
        "Загрузка описания...");
    descriptionBrowser->setStyleSheet(QString(
                                          "QTextBrowser { background-color: %1; border: 1px solid %2;"
                                          " border-radius: 10px; padding: 12px; color: %3; }")
                                          .arg(kPanel, kBorder, kText));

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
            "+ Установить мод",
            this);
    installButton->setFixedHeight(44);
    installButton->setCursor(Qt::PointingHandCursor);
    installButton->setStyleSheet(QString(
                                     "QPushButton { background-color: %1; color: #0A0A0A; border: none;"
                                     " border-radius: 10px; font-weight: bold; font-size: 15px; }"
                                     "QPushButton:hover { background-color: #15c25e; }")
                                     .arg(kAccent));

    connect(installButton, &QPushButton::clicked, this, [this]() {
        emit installRequested(currentMod);
    });

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