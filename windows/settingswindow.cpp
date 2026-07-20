#include "settingswindow.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QSettings>

#include "../settings.h"
#include "../ui/theme.h"

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Настройки");
    resize(480, 420);
    setStyleSheet(Theme::dialogStyle() + R"(
        QCheckBox { color: #E8EAED; }
        QCheckBox::indicator {
            width: 18px; height: 18px;
            border: 1px solid #3A3E45;
            border-radius: 4px;
            background: #26292F;
        }
        QCheckBox::indicator:checked {
            background: #1BD96A;
            border-color: #1BD96A;
        }
        QSlider::groove:horizontal {
            background: #3A3E45; border-radius: 4px; height: 6px;
        }
        QSlider::handle:horizontal {
            background: #1BD96A; border-radius: 8px;
            width: 16px; height: 16px; margin: -5px 0;
        }
        QSlider::sub-page:horizontal { background: #1BD96A; border-radius: 4px; }
        QPushButton#saveBtn {
            background-color: #1BD96A; color: #0A0A0A;
            border: none; border-radius: 8px;
            font-weight: bold; font-size: 14px;
            min-height: 40px;
        }
        QPushButton#saveBtn:hover { background-color: #15C25E; }
    )");

    auto* layout = new QVBoxLayout(this);

    // Снапшоты
    snapshotsCheckBox = new QCheckBox("Показывать снапшоты", this);
    snapshotsCheckBox->setChecked(globalSettings.showSnapshots);

    // Память
    ramLabel = new QLabel(this);
    ramSlider = new QSlider(Qt::Horizontal, this);
    connect(ramSlider, &QSlider::valueChanged, this, [this](int value) {
        ramLabel->setText(QString("Оперативная память: %1 ГБ").arg(value));
        emit settingsChanged();
    });
    ramSlider->setMinimum(1);
    ramSlider->setMaximum(32);
    ramSlider->setValue(globalSettings.ramGb);

    // Никнейм
    auto* usernameLabel = new QLabel("Никнейм в игре:", this);
    nicknameEdit = new QLineEdit(this);
    nicknameEdit->setText(globalSettings.nickname);

    // Путь к Minecraft
    auto* minecraftPathLabel = new QLabel("Путь к Minecraft:", this);
    minecraftPathEdit = new QLineEdit(this);
    minecraftBrowseButton = new QPushButton("Обзор...", this);
    minecraftPathEdit->setText(globalSettings.minecraftPath);

    connect(minecraftBrowseButton, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Minecraft");
        if (!dir.isEmpty()) {
            minecraftPathEdit->setText(dir);
            emit settingsChanged();
        }
    });

    // Путь к Java
    auto* javaPathLabel = new QLabel("Путь к Java:", this);
    javaPathEdit = new QLineEdit(this);
    javaBrowseButton = new QPushButton("Обзор...", this);
    javaPathEdit->setText(globalSettings.javaPath);

    connect(javaBrowseButton, &QPushButton::clicked, this, [this]{
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Java");
        if (!dir.isEmpty()) {
            javaPathEdit->setText(dir);
            emit settingsChanged();
        }
    });


    // Кнопка сохранения
    auto* closeButton = new QPushButton("Сохранить и закрыть", this);
    closeButton->setObjectName("saveBtn");

    // Добавляем всё в layout
    layout->addWidget(snapshotsCheckBox);
    layout->addWidget(ramLabel);
    layout->addWidget(ramSlider);

    layout->addWidget(usernameLabel);
    layout->addWidget(nicknameEdit);

    layout->addWidget(minecraftPathLabel);
    layout->addWidget(minecraftPathEdit);
    layout->addWidget(minecraftBrowseButton);

    layout->addWidget(javaPathLabel);
    layout->addWidget(javaPathEdit);
    layout->addWidget(javaBrowseButton);

    layout->addStretch();
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, [this]() {
        globalSettings.showSnapshots = snapshotsCheckBox->isChecked();
        globalSettings.javaPath = javaPathEdit->text();
        globalSettings.minecraftPath = minecraftPathEdit->text();
        globalSettings.nickname = nicknameEdit->text();
        globalSettings.ramGb = ramSlider->value();
        emit settingsChanged();
        accept();
    });
}
