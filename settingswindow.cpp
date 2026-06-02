#include "settingswindow.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QSettings>

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Настройки");
    resize(450, 320);

    auto* layout = new QVBoxLayout(this);

    // Снапшоты
    snapshotsCheckBox = new QCheckBox("Показывать снапшоты", this);
    snapshotsCheckBox->setChecked(settings.value("snapshots", false).toBool());

    // Память
    ramLabel = new QLabel(this);
    ramSlider = new QSlider(Qt::Horizontal, this);
    connect(ramSlider, &QSlider::valueChanged, this, [this](int value) {
        ramLabel->setText(QString("Оперативная память: %1 ГБ").arg(value));
        emit settingsChanged();
    });
    ramSlider->setMinimum(1);
    ramSlider->setMaximum(32);
    ramSlider->setValue(settings.value("minecraftRam", 2).toInt()); // 2 ГБ по умолчанию

    // Путь к Minecraft
    QLabel* minecraftPathLabel = new QLabel("Путь к Minecraft:", this);
    minecraftPathEdit = new QLineEdit(this);
    minecraftBrowseButton = new QPushButton("Обзор...", this);

    // Загружаем сохранённый путь
    minecraftPathEdit->setText(settings.value("minecraftPath",
                                     "C:/Users/" + qgetenv("USERNAME") + "/AppData/Roaming/.minecraft").toString());

    connect(minecraftBrowseButton, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Minecraft");
        if (!dir.isEmpty()) {
            minecraftPathEdit->setText(dir);
            emit settingsChanged();
        }
    });

    QLabel* javaPathLabel = new QLabel("Путь к Java:", this);
    javaPathEdit = new QLineEdit(this);
    javaBrowseButton = new QPushButton("Обзор...", this);
    javaPathEdit->setText(settings.value("javaPath", "C:/Program Files/Java/jdk-21/").toString());

    connect(javaBrowseButton, &QPushButton::clicked, this, [this]{
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Java");
        if (!dir.isEmpty()) {
            javaPathEdit->setText(dir);
            emit settingsChanged();
        }
    });

    connect(snapshotsCheckBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) {
        emit settingsChanged();
    });

    // Кнопка закрытия
    QPushButton* closeButton = new QPushButton("Сохранить и закрыть", this);

    layout->addWidget(snapshotsCheckBox);
    layout->addWidget(ramLabel);
    layout->addWidget(ramSlider);
    layout->addWidget(minecraftPathLabel);
    layout->addWidget(minecraftPathEdit);
    layout->addWidget(minecraftBrowseButton);
    layout->addWidget(javaPathLabel);
    layout->addWidget(javaPathEdit);
    layout->addWidget(javaBrowseButton);
    layout->addStretch();
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, [this]() {
        // Сохраняем путь
        settings.setValue("minecraftPath", minecraftPathEdit->text());
        settings.setValue("javaPath", javaPathEdit->text());
        settings.setValue("snapshots", snapshotsCheckBox->isChecked());
        settings.setValue("minecraftRam", ramSlider->value());
        accept();
    });
}

bool SettingsWindow::showSnapshots() const
{
    return settings.value("snapshots", false).toBool();
}

int SettingsWindow::ramAmount() const
{
    return settings.value("minecraftRam", 0).toInt();
}

QString SettingsWindow::minecraftPath() const
{
    return settings.value("minecraftPath").toString();
}

QString SettingsWindow::javaPath() const{
    return settings.value("javaPath").toString() + "/bin/javaw.exe";
}