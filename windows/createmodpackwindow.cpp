#include "createmodpackwindow.h"
#include "settingswindow.h"
#include "../ui/windowframe.h"
#include "../ui/theme.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDir>
#include <QSet>
#include <QVersionNumber>

#include "../settings.h"

CreateModpackWindow::CreateModpackWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    resize(500, 320);

    m_frame = new WindowFrame(this);

    m_frame->setTitle("Создание сборки");

    setStyleSheet(Theme::dialogStyle());

    auto* rootLayout =
        new QVBoxLayout(this);

    rootLayout->setContentsMargins(0,0,0,0);
    rootLayout->setSpacing(0);

    rootLayout->addWidget(m_frame);

    auto* layout =
        new QVBoxLayout(m_frame->contentWidget());

    layout->addWidget(
        new QLabel("Название сборки"));

    m_nameEdit =
        new QLineEdit(this);

    layout->addWidget(m_nameEdit);

    layout->addWidget(
        new QLabel("Версия Minecraft"));

    m_versionBox =
        new QComboBox(this);

    layout->addWidget(m_versionBox);

    layout->addWidget(
        new QLabel("Загрузчик"));

    m_loaderBox =
        new QComboBox(this);

    m_loaderBox->addItems({
        "Vanilla",
        "Fabric",
        "Forge",
        "NeoForge"
    });

    layout->addWidget(m_loaderBox);

    layout->addWidget(
        new QLabel("Версия загрузчика"));

    m_loaderVersionBox =
        new QComboBox(this);

    layout->addWidget(m_loaderVersionBox);

    m_createButton =
        new QPushButton(
            "Создать сборку");
    m_createButton->setStyleSheet(Theme::accentButtonStyle());
    m_createButton->setMinimumHeight(40);

    layout->addWidget(m_createButton);

    connect(
        m_createButton,
        &QPushButton::clicked,
        this,
        &CreateModpackWindow::onCreate);

    connect(
        m_loaderBox,
        &QComboBox::currentTextChanged,
        this,
        &CreateModpackWindow::loadLoaderVersions);
}

void CreateModpackWindow::setSettingsWindow(
    SettingsWindow* settings)
{
    m_settingsWindow = settings;
}
//
// void CreateModpackWindow::setDownloader(
//     MinecraftDownloader* d)
// {
//     if(!d)
//         return;
//
//     m_downloader = d;
//
//     connect(
//         m_downloader,
//         &MinecraftDownloader::vanillaVersionsReceived,
//         this,
//         &CreateModpackWindow::onVersionsLoaded);
//
//     connect(
//         m_downloader,
//         &MinecraftDownloader::fabricVersionsReceived,
//         this,
//         &CreateModpackWindow::onFabricVersions);
//
//     connect(
//         m_downloader,
//         &MinecraftDownloader::forgeVersionsReceived,
//         this,
//         &CreateModpackWindow::onForgeVersions);
//
//     connect(
//         m_downloader,
//         &MinecraftDownloader::neoforgeVersionReceived,
//         this,
//         &CreateModpackWindow::onNeoForgeVersions);
//
//     m_mnifestDownloader->fetchVanillaVersions();
// }
//
// void CreateModpackWindow::loadLoaderVersions()
// {
//     if(!m_downloader)
//         return;
//
//     m_loaderVersionBox->clear();
//
//     QString loader =
//         m_loaderBox->currentText();
//
//     if(loader == "Fabric")
//     {
//         m_downloader->md.fetchFabricVersions();
//     }
//     else if(loader == "Forge")
//     {
//         m_downloader->md.fetchForgeVersions();
//     }
//     else if(loader == "NeoForge")
//     {
//         m_downloader->md.fetchNeoForgeVersions();
//     }
// }

void CreateModpackWindow::onVersionsLoaded(
    const QVector<MinecraftVersion>& versions)
{
    m_versionBox->clear();

    for(const auto& v : versions)
    {
        // Показываем только release-версии (снапшоты не нужны при создании сборки)
        if(v.m_loaderType == "release")
            m_versionBox->addItem(v.m_gameVersion);
    }

    qDebug() << "CreateModpackWindow: loaded" << m_versionBox->count() << "release versions";
}

void CreateModpackWindow::onFabricVersions(
    const QJsonArray& versions)
{
    m_loaderVersionBox->clear();

    for(const auto& value : versions)
    {
        QJsonObject obj =
            value.toObject();

        m_loaderVersionBox->addItem(
            obj["version"]
                .toString());
    }
}

void CreateModpackWindow::onForgeVersions(
    const QJsonObject& json)
{
    m_loaderVersionBox->clear();

    QJsonObject promos =
        json["promos"]
            .toObject();

    QString mcVersion =
        m_versionBox->currentText();

    QString key =
        mcVersion + "-latest";

    if(promos.contains(key))
    {
        m_loaderVersionBox->addItem(
            promos[key]
                .toString());
    }
}

void CreateModpackWindow::onNeoForgeVersions(
    const QString& xml)
{
    m_loaderVersionBox->clear();

    QStringList lines =
        xml.split('\n');

    for(const QString& line : lines)
    {
        if(!line.contains("<version>"))
            continue;

        QString version =
            line;

        version.remove("<version>");
        version.remove("</version>");
        version = version.trimmed();

        m_loaderVersionBox->addItem(
            version);
    }
}

void CreateModpackWindow::onCreate()
{
    if(!m_downloader)
        return;

    QString name =
        m_nameEdit->text().trimmed();

    if(name.isEmpty())
    {
        QMessageBox::warning(
            this,
            "Ошибка",
            "Введите название сборки");

        return;
    }

    QString basePath = globalSettings.minecraftPath;

    if(basePath.isEmpty())
    {
        QMessageBox::warning(
            this,
            "Ошибка",
            "Укажите путь Minecraft в настройках");

        return;
    }

    QString instancePath =
        QDir(basePath)
            .filePath(name);

    QString version =
        m_versionBox->currentText();

    QString loader =
        m_loaderBox->currentText()
            .toLower();

    if(loader == "vanilla")
        loader.clear();

    QString loaderVersion =
        m_loaderVersionBox->currentText();

    //m_downloader->createInstance(
    //    version,
    //    loader,
    //    loaderVersion,
    //    instancePath);

    QMessageBox::information(
        this,
        "Создание",
        "Создание сборки начато");

    accept();
}