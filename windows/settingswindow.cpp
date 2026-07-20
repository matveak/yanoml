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
    m_snapshotsCheckBox = new QCheckBox("Показывать снапшоты", this);
    m_snapshotsCheckBox->setChecked(globalSettings.showSnapshots);

    // Память
    m_ramLabel = new QLabel(this);
    m_ramSlider = new QSlider(Qt::Horizontal, this);
    connect(m_ramSlider, &QSlider::valueChanged, this, [this](int value) {
        m_ramLabel->setText(QString("Оперативная память: %1 ГБ").arg(value));
        emit settingsChanged();
    });
    m_ramSlider->setMinimum(1);
    m_ramSlider->setMaximum(32);
    m_ramSlider->setValue(globalSettings.ramGb);

    // Никнейм
    auto* usernameLabel = new QLabel("Никнейм в игре:", this);
    m_nicknameEdit = new QLineEdit(this);
    m_nicknameEdit->setText(globalSettings.nickname);

    // Путь к Minecraft
    auto* minecraftPathLabel = new QLabel("Путь к Minecraft:", this);
    m_minecraftPathEdit = new QLineEdit(this);
    m_minecraftBrowseButton = new QPushButton("Обзор...", this);
    m_minecraftPathEdit->setText(globalSettings.minecraftPath);

    connect(m_minecraftBrowseButton, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Minecraft");
        if (!dir.isEmpty()) {
            m_minecraftPathEdit->setText(dir);
            emit settingsChanged();
        }
    });

    // Путь к Java
    auto* javaPathLabel = new QLabel("Путь к Java:", this);
    m_javaPathEdit = new QLineEdit(this);
    m_javaBrowseButton = new QPushButton("Обзор...", this);
    m_javaPathEdit->setText(globalSettings.javaPath);

    connect(m_javaBrowseButton, &QPushButton::clicked, this, [this]{
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Java");
        if (!dir.isEmpty()) {
            m_javaPathEdit->setText(dir);
            emit settingsChanged();
        }
    });


    // Кнопка сохранения
    auto* closeButton = new QPushButton("Сохранить и закрыть", this);
    closeButton->setObjectName("saveBtn");

    // Добавляем всё в layout
    layout->addWidget(m_snapshotsCheckBox);
    layout->addWidget(m_ramLabel);
    layout->addWidget(m_ramSlider);

    layout->addWidget(usernameLabel);
    layout->addWidget(m_nicknameEdit);

    layout->addWidget(minecraftPathLabel);
    layout->addWidget(m_minecraftPathEdit);
    layout->addWidget(m_minecraftBrowseButton);

    layout->addWidget(javaPathLabel);
    layout->addWidget(m_javaPathEdit);
    layout->addWidget(m_javaBrowseButton);

    layout->addStretch();
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, [this] {
        globalSettings.showSnapshots = m_snapshotsCheckBox->isChecked();
        globalSettings.javaPath = m_javaPathEdit->text();
        globalSettings.minecraftPath = m_minecraftPathEdit->text();
        globalSettings.nickname = m_nicknameEdit->text();
        globalSettings.ramGb = m_ramSlider->value();
        emit settingsChanged();
        accept();
    });
}
