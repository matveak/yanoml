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

#include "../ui/theme.h"

ModDetailsWindow::ModDetailsWindow(
    const Mod& mod,
    QWidget* parent)
    : QDialog(parent),
    m_currentMod(mod)
{
    resize(1000, 750);
    setWindowTitle(mod.name);
    setStyleSheet(Theme::dialogStyle());

    m_api = new ModrithAPI(this);
    m_manager = new QNetworkAccessManager(this);

    auto mainLayout =
        new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(14);

    // =========================
    // HEADER
    // =========================

    auto* headerLayout =
        new QHBoxLayout();

    m_iconLabel =
        new QLabel(this);

    m_iconLabel->setFixedSize(128, 128);
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setStyleSheet(QString(
                                 "border-radius: 12px; border: 1px solid %1; background: %2;")
                                 .arg(Theme::border().name(), Theme::panel().name()));

    auto* infoLayout =
        new QVBoxLayout();

    infoLayout->setSpacing(6);

    m_titleLabel =
        new QLabel(
            "<h1>" + mod.name + "</h1>");
    m_titleLabel->setStyleSheet(QString("font-size: 26px; color: %1;").arg(Theme::text().name()));

    m_authorLabel =
        new QLabel(
            "Автор: " + mod.author);
    m_authorLabel->setStyleSheet(QString("color: %1;").arg(Theme::textDim().name()));

    m_downloadsLabel =
        new QLabel(
            "⬇ " +
            QString::number(mod.downloads));
    m_downloadsLabel->setStyleSheet(QString("color: %1;").arg(Theme::textDim().name()));

    m_categoriesLabel =
        new QLabel();
    m_categoriesLabel->setWordWrap(true);
    m_categoriesLabel->setStyleSheet(QString("color: %1;").arg(Theme::textDim().name()));

    m_versionsLabel =
        new QLabel(
            "Версии: " +
            mod.versions.join(", "));
    m_versionsLabel->setWordWrap(true);
    m_versionsLabel->setStyleSheet(QString("color: %1;").arg(Theme::textDim().name()));

    infoLayout->addWidget(m_titleLabel);
    infoLayout->addWidget(m_authorLabel);
    infoLayout->addWidget(m_downloadsLabel);
    infoLayout->addWidget(m_categoriesLabel);
    infoLayout->addWidget(m_versionsLabel);
    infoLayout->addStretch(1);

    headerLayout->addWidget(m_iconLabel, 0, Qt::AlignTop);
    headerLayout->addSpacing(16);
    headerLayout->addLayout(infoLayout, 1);

    // =========================
    // DESCRIPTION
    // =========================

    m_descriptionBrowser =
        new QTextBrowser(this);

    m_descriptionBrowser->setOpenExternalLinks(true);
    m_descriptionBrowser->setText(
        "Загрузка описания...");
    m_descriptionBrowser->setStyleSheet(QString(
                                          "QTextBrowser { background-color: %1; border: 1px solid %2;"
                                          " border-radius: 10px; padding: 12px; color: %3; }")
                                          .arg(Theme::panel().name(), Theme::border().name(), Theme::text().name()));

    // =========================
    // GALLERY
    // =========================

    auto galleryArea =
        new QScrollArea(this);

    galleryArea->setWidgetResizable(true);

    auto* galleryWidget =
        new QWidget();

    m_galleryLayout =
        new QVBoxLayout(galleryWidget);

    galleryArea->setWidget(galleryWidget);

    // =========================
    // INSTALL BUTTON
    // =========================

    m_installButton =
        new QPushButton(
            "+ Установить мод",
            this);
    m_installButton->setFixedHeight(44);
    m_installButton->setCursor(Qt::PointingHandCursor);
    m_installButton->setStyleSheet(QString(
                                     "QPushButton { background-color: %1; color: #0A0A0A; border: none;"
                                     " border-radius: 10px; font-weight: bold; font-size: 15px; }"
                                     "QPushButton:hover { background-color: #15c25e; }")
                                     .arg(Theme::accent().name()));

    connect(m_installButton, &QPushButton::clicked, this, [this] {
        emit installRequested(m_currentMod);
    });

    // =========================
    // ADD TO LAYOUT
    // =========================

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_descriptionBrowser, 1);
    mainLayout->addWidget(galleryArea, 1);
    mainLayout->addWidget(m_installButton);

    // =========================
    // SIGNALS
    // =========================

    connect(
        m_api,
        &ModrithAPI::ProjectReceived,
        this,
        &ModDetailsWindow::onProjectReceived);

    connect(
        m_api,
        &ModrithAPI::OnError,
        this,
        [](const QString& error)
        {
            qDebug() << error;
        });

    m_api->getProject(
        m_currentMod.id);
}

void ModDetailsWindow::onProjectReceived(
    const ModProject& project)
{
    m_titleLabel->setText(
        "<h1>" +
        project.title +
        "</h1>");

    m_authorLabel->setText(
        "Автор: " +
        project.author);

    m_downloadsLabel->setText(
        "Загрузок: " +
        QString::number(project.downloads));

    m_categoriesLabel->setText(
        "Категории: " +
        project.categories.join(", "));

#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
    m_descriptionBrowser->setMarkdown(
        project.body);
#else
    m_descriptionBrowser->setPlainText(
        project.body);
#endif

    // =========================
    // ICON
    // =========================

    if(!project.iconUrl.isEmpty())
    {
        QNetworkReply* reply =
            m_manager->get(
                QNetworkRequest(
                    QUrl(project.iconUrl)));

        connect(
            reply,
            &QNetworkReply::finished,
            this,
            [this, reply]
            {
                QByteArray data =
                    reply->readAll();

                QPixmap pix;

                if(pix.loadFromData(data))
                {
                    m_iconLabel->setPixmap(
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
        auto imageLabel =
            new QLabel();

        imageLabel->setAlignment(
            Qt::AlignCenter);

        imageLabel->setMinimumHeight(
            250);

        imageLabel->setText(
            "Загрузка изображения...");

        m_galleryLayout->addWidget(
            imageLabel);

        QNetworkReply* reply =
            m_manager->get(
                QNetworkRequest(
                    QUrl(imageUrl)));

        connect(
            reply,
            &QNetworkReply::finished,
            this,
            [imageLabel, reply]
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