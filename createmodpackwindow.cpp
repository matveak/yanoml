#include "createmodpackwindow.h"
#include "settingswindow.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDir>
#include <QSet>
#include <QVersionNumber>

CreateModpackWindow::CreateModpackWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Создание сборки");
    resize(500, 300);

    QVBoxLayout* layout =
        new QVBoxLayout(this);

    layout->addWidget(
        new QLabel("Название сборки"));

    nameEdit =
        new QLineEdit(this);

    layout->addWidget(nameEdit);

    layout->addWidget(
        new QLabel("Версия Minecraft"));

    versionBox =
        new QComboBox(this);

    layout->addWidget(versionBox);

    layout->addWidget(
        new QLabel("Загрузчик"));

    loaderBox =
        new QComboBox(this);

    loaderBox->addItems({
        "Vanilla",
        "Fabric",
        "Forge",
        "NeoForge"
    });

    layout->addWidget(loaderBox);

    layout->addWidget(
        new QLabel("Версия загрузчика"));

    loaderVersionBox =
        new QComboBox(this);

    layout->addWidget(loaderVersionBox);

    createButton =
        new QPushButton(
            "Создать сборку");

    layout->addWidget(createButton);

    connect(
        createButton,
        &QPushButton::clicked,
        this,
        &CreateModpackWindow::onCreate);

    connect(
        loaderBox,
        &QComboBox::currentTextChanged,
        this,
        &CreateModpackWindow::loadLoaderVersions);
}

void CreateModpackWindow::setSettingsWindow(
    SettingsWindow* settings)
{
    settingsWindow = settings;
}

void CreateModpackWindow::setDownloader(
    MinecraftDownloader* d)
{
    if(!d)
        return;

    downloader = d;

    connect(
        downloader,
        &MinecraftDownloader::vanillaVersionsReceived,
        this,
        &CreateModpackWindow::onVersionsLoaded);

    connect(
        downloader,
        &MinecraftDownloader::fabricVersionsReceived,
        this,
        &CreateModpackWindow::onFabricVersions);

    connect(
        downloader,
        &MinecraftDownloader::forgeVersionsReceived,
        this,
        &CreateModpackWindow::onForgeVersions);

    connect(
        downloader,
        &MinecraftDownloader::neoforgeVersionReceived,
        this,
        &CreateModpackWindow::onNeoForgeVersions);

    downloader->fetchVanillaVersions();
}

void CreateModpackWindow::loadLoaderVersions()
{
    if(!downloader)
        return;

    loaderVersionBox->clear();

    QString loader =
        loaderBox->currentText();

    if(loader == "Fabric")
    {
        downloader->fetchFabricVersions();
    }
    else if(loader == "Forge")
    {
        downloader->fetchForgeVersions();
    }
    else if(loader == "NeoForge")
    {
        downloader->fetchNeoForgeVersions();
    }
}

void CreateModpackWindow::onVersionsLoaded(
    const QVector<MinecraftVersion>& versions)
{
    versionBox->clear();

    for(const auto& v : versions)
    {
        // Показываем только release-версии (снапшоты не нужны при создании сборки)
        if(v.loaderType == "release")
            versionBox->addItem(v.gameVersion);
    }

    qDebug() << "CreateModpackWindow: loaded" << versionBox->count() << "release versions";
}

void CreateModpackWindow::onFabricVersions(
    const QJsonArray& versions)
{
    loaderVersionBox->clear();

    for(const auto& value : versions)
    {
        QJsonObject obj =
            value.toObject();

        loaderVersionBox->addItem(
            obj["version"]
                .toString());
    }
}

void CreateModpackWindow::onForgeVersions(
    const QJsonObject& json)
{
    loaderVersionBox->clear();

    QJsonObject promos =
        json["promos"]
            .toObject();

    QString mcVersion =
        versionBox->currentText();

    QString key =
        mcVersion + "-latest";

    if(promos.contains(key))
    {
        loaderVersionBox->addItem(
            promos[key]
                .toString());
    }
}

void CreateModpackWindow::onNeoForgeVersions(
    const QString& xml)
{
    loaderVersionBox->clear();

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

        loaderVersionBox->addItem(
            version);
    }
}

void CreateModpackWindow::onCreate()
{
    if(!downloader)
        return;

    QString name =
        nameEdit->text().trimmed();

    if(name.isEmpty())
    {
        QMessageBox::warning(
            this,
            "Ошибка",
            "Введите название сборки");

        return;
    }

    if(!settingsWindow)
    {
        QMessageBox::warning(
            this,
            "Ошибка",
            "Настройки не подключены");

        return;
    }

    QString basePath =
        settingsWindow->minecraftPath();

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
        versionBox->currentText();

    QString loader =
        loaderBox->currentText()
            .toLower();

    if(loader == "vanilla")
        loader.clear();

    QString loaderVersion =
        loaderVersionBox->currentText();

    downloader->createInstance(
        version,
        loader,
        loaderVersion,
        instancePath);

    QMessageBox::information(
        this,
        "Создание",
        "Создание сборки начато");

    accept();
}