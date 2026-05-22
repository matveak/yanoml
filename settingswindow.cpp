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

    // Память
    ramLabel = new QLabel("Оперативная память: 1024 МБ", this);
    ramSlider = new QSlider(Qt::Horizontal, this);
    ramSlider->setMinimum(1);
    ramSlider->setMaximum(32);
    ramSlider->setValue(2); // 2 ГБ по умолчанию

    // Путь к Minecraft
    QLabel* pathLabel = new QLabel("Путь к Minecraft:", this);
    pathEdit = new QLineEdit(this);
    browseButton = new QPushButton("Обзор...", this);

    // Загружаем сохранённый путь
    QSettings settings("MyLauncher", "Crack");
    pathEdit->setText(settings.value("minecraftPath",
                                     "C:/Users/" + qgetenv("USERNAME") + "/AppData/Roaming/.minecraft").toString());

    connect(browseButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Minecraft");
        if (!dir.isEmpty()) {
            pathEdit->setText(dir);
            emit settingsChanged();
        }
    });

    connect(ramSlider, &QSlider::valueChanged, this, [this](int value) {
        ramLabel->setText(QString("Оперативная память: %1 ГБ").arg(value));
        emit settingsChanged();
    });

    connect(snapshotsCheckBox, &QCheckBox::stateChanged, this, [this](int) {
        emit settingsChanged();
    });

    // Кнопка закрытия
    QPushButton* closeButton = new QPushButton("Сохранить и закрыть", this);

    layout->addWidget(snapshotsCheckBox);
    layout->addWidget(ramLabel);
    layout->addWidget(ramSlider);
    layout->addWidget(pathLabel);
    layout->addWidget(pathEdit);
    layout->addWidget(browseButton);
    layout->addStretch();
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, [this]() {
        // Сохраняем путь
        QSettings settings("MyLauncher", "Crack");
        settings.setValue("minecraftPath", pathEdit->text());
        accept();
    });
}

SettingsWindow::~SettingsWindow() = default;

bool SettingsWindow::showSnapshots() const
{
    return snapshotsCheckBox && snapshotsCheckBox->isChecked();
}

int SettingsWindow::ramAmount() const
{
    return ramSlider ? ramSlider->value() * 1024 : 2048;
}

QString SettingsWindow::minecraftPath() const
{
    return pathEdit ? pathEdit->text() : "";
}